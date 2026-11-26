#include "GranularVisualizer.h"
#include "../DSP/GranularProcessor.h"
#include <algorithm>
#include <cmath>
namespace granular {
namespace {
float grainLight(const GrainEvent& event,float envelope) noexcept {
 if(!std::isfinite(event.sourceLevel)||event.sourceLevel<=0) return 0;
 // Compress the visual range so a soft bass layer still has visible nuclei.
 const float sourceGlow=1.f-std::exp(-13.f*event.sourceLevel);
 return sourceGlow*std::sqrt(std::clamp(event.wet,0.f,1.f)*std::clamp(envelope,0.f,1.f));
}

juce::Point<float> grainTarget(const GrainEvent& event,float width,float height) noexcept {
 return {width*(.5f+.35f*event.pan+.22f*(.5f-event.readOffset)),
         height*(.48f-.30f*event.pitch/24.f)};
}

juce::Image makeBloom() {
 juce::Image image(juce::Image::ARGB,64,64,true,juce::SoftwareImageType());
 juce::Image::BitmapData pixels(image,juce::Image::BitmapData::writeOnly);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
  const float radius=std::hypot(x-31.5f,y-31.5f)/31.5f;
  const float falloff=std::clamp(1.f-radius,0.f,1.f);
  pixels.setPixelColour(x,y,juce::Colours::white.withAlpha(falloff*falloff*falloff));
 }
 return image;
}

void drawBloom(juce::Graphics& g,const juce::Image& bloom,juce::Point<float> centre,
               float radius,juce::Colour colour) {
 g.setColour(colour);
 g.drawImage(bloom,juce::Rectangle<float>(radius*2,radius*2).withCentre(centre),
             juce::RectanglePlacement::stretchToFit,true);
}
}

void GrainVisualModel::ingest(VisualTelemetry& t) noexcept {
 GrainEvent e;
 for(int n=0;n<4096 && t.grains.pop(e);++n){
  clock=std::max(clock,e.sampleStamp);
  if(e.generation<generation)continue;
  if(e.generation!=generation){clear();generation=e.generation;}
  if(e.kind==GrainEventKind::Reset){clear();continue;}
  if(e.slot>=particles.size())continue;
  auto& p=particles[e.slot];
  if(e.id<p.metadata.id)continue;
  if(e.kind==GrainEventKind::End){if(e.id==p.metadata.id)p.active=false;continue;}
  if(e.id!=p.metadata.id)p.placed=false;
  p.metadata=e;p.active=true;
  const float light=grainLight(e,e.envelopeGain);
  if(light>p.pendingPeak){p.pendingPeak=light;p.peakEvent=e;}
 }
 clock=std::max(clock,t.audioClock.load(std::memory_order_acquire));
 for(auto& p:particles) {
  if(p.active && (clock<p.metadata.birthSample || clock-p.metadata.birthSample>=std::uint64_t(p.metadata.length)))p.active=false;
  const auto& flash=p.peakEvent;
  // Discard old flashes after a closed editor or a full telemetry queue.
  if(clock>flash.sampleStamp&&double(clock-flash.sampleStamp)>.09*flash.sampleRate)p.pendingPeak=0;
 }
}
float GrainVisualModel::phase(const Particle& p) const noexcept {
 if(clock<=p.metadata.birthSample)return 0;
 return std::clamp(float(clock-1-p.metadata.birthSample)/float(std::max(1,p.metadata.length-1)),0.f,1.f);
}
float GrainVisualModel::envelope(const Particle& p) const noexcept {
 return GranularProcessor::windowEnvelope(phase(p),p.metadata.windowMorph,p.metadata.windowEdge);
}
int GrainVisualModel::activeCount() const noexcept {int n=0;for(auto& p:particles)n+=p.active;return n;}
GranularVisualizer::GranularVisualizer(VisualTelemetry& t):telemetry(t),bloom(makeBloom()){
 setInterceptsMouseClicks(false,false);setOpaque(false);
 lastFrame=lastClockChange=juce::Time::getMillisecondCounterHiRes()*.001;
 for(int n=0;n<8;++n){motion[n].reset(60,.8);motion[n].setCurrentAndTargetValue(n==2?1.f:0.f);}
 juce::Random random(671928);
 for(auto& p:flow) {
  const float u=random.nextFloat();
  p={u,random.nextFloat()+random.nextFloat()+random.nextFloat()-1.5f,
     random.nextFloat()*6.2831853f,.55f+random.nextFloat()*1.5f,random.nextFloat(),
     std::pow(std::max(0.f,std::sin(juce::MathConstants<float>::pi*u)),.7f)};
 }
 startTimerHz(60);
}
GranularVisualizer::~GranularVisualizer(){stopTimer();}
void GranularVisualizer::setTheme(juce::Colour b,juce::Colour a,juce::Colour s,int m){
 background=b;accent=a;secondary=s;m=std::clamp(m,0,7);if(m==mood)return;mood=m;
 for(int n=0;n<8;++n)motion[n].setTargetValue(n==m?1.f:0.f);
}
void GranularVisualizer::refreshNow(){
 const double now=juce::Time::getMillisecondCounterHiRes()*.001;
 const float dt=float(std::clamp(now-lastFrame,0.001,0.05));lastFrame=now;time+=dt;
 model.ingest(telemetry);
 if(model.clock!=lastClock){lastClock=model.clock;lastClockChange=now;}
 const bool idle=now-lastClockChange>.20;
 if(idle)model.clear();
 ScopePoint point;
 int samples=0;
 for(;samples<2048&&telemetry.scope.pop(point);++samples)history[historyWrite++%history.size()]=point;
 if(samples>0||idle) {
  const int recent=std::min(samples,24);
  float energy=0,peak=0;
  for(int n=0;n<recent;++n) {
   const float level=history[(historyWrite-1-unsigned(n))%history.size()].rms;
   if(!std::isfinite(level))continue;
   const float bounded=std::clamp(level,0.f,4.f);
   energy+=bounded*bounded;
   peak=std::max(peak,bounded);
  }
  const float target=recent>0?std::sqrt(energy/float(recent)):0.f;
  // Compare the newest peak with the slower envelope to catch attacks between frames.
  const float onset=std::clamp((peak-slowRms*1.6f-.015f)*4.f,0.f,1.f);
  if(onset>.12f&&onset>impact+.06f)impactAge=0;
  impact=std::max(onset,impact*std::exp(-dt*10.f));
  rms+=(target-rms)*(1-std::exp(-dt*(target>rms?85.f:16.f)));
  slowRms+=(target-slowRms)*(1-std::exp(-dt*7.f));
 } else {
  impact*=std::exp(-dt*10.f);
 }
 impactAge+=dt;
 for(auto& s:motion)s.getNextValue();
 const float anxious=motion[1].getCurrentValue(),calm=motion[3].getCurrentValue();
 constexpr float spring[]={14,114,20,10,18,24,8,100},drag[]={14,7,9,17,11,10,19,6};
 float stiffness=0,damping=0;for(int n=0;n<8;++n){const float weight=std::clamp(motion[n].getCurrentValue(),0.f,1.f);stiffness+=spring[n]*weight;damping+=drag[n]*weight;}
 const float width=float(getWidth()),height=float(getHeight());
 for(auto& p:model.particles){
  p.trailEnergy*=std::exp(-dt*24.f);
  if(p.pendingPeak>.006f) {
   const auto& hit=p.peakEvent;
   const auto position=grainTarget(hit,width,height);
   p.trailOrigin=position;
   p.trailEnd=position+juce::Point<float>(-18.f-hit.pan*20.f,hit.pitch*.27f);
   p.trailColour=juce::Colour(hit.colour);
   p.trailEnergy=p.pendingPeak;
  }
  p.pendingPeak=0;
  if(!p.active)continue;
  const auto& e=p.metadata;const float phase=model.phase(p);
  const auto target=grainTarget(e,width,height);
  const float x=target.x;
  const float y=target.y-10*calm*phase;
  const float jitter=anxious*4*std::sin(time*130+float(e.id%31));
  if(!p.placed){p.x=x;p.y=y;p.vx=p.vy=0;p.placed=true;}
  p.vx+=((x+jitter-p.x)*stiffness-p.vx*damping)*dt;
  p.vy+=((y+jitter*.7f-p.y)*stiffness-p.vy*damping)*dt;
  p.x+=p.vx*dt;p.y+=p.vy*dt;
 }
 repaint();
}
void GranularVisualizer::paint(juce::Graphics& g) {
 const double began=juce::Time::getMillisecondCounterHiRes();
 const float width=float(getWidth()),height=float(getHeight());
 if(width<=1||height<=1)return;

 const float level=1.f-std::exp(-5.f*rms);
 const float swell=level+.85f*impact;
 const float motionTime=time*.6f;
 const auto wavePoint=[&](float u,float y){return juce::Point<float>(width*(.035f+.93f*u),y);};
 const auto wavePulse=[&](float u) {
  const float distance=std::abs(u-.5f);
  const float front=impactAge*1.7f;
  return impact*std::max(0.f,1.f-std::abs(distance-front)*13.f);
 };

 constexpr int ribbonSamples=128;
 std::array<float,ribbonSamples> centres{}, amplitudes{};
 for(int n=0;n<ribbonSamples;++n) {
  const float u=float(n)/(ribbonSamples-1);
  const float edge=std::sin(juce::MathConstants<float>::pi*u);
  const auto& point=history[(historyWrite+unsigned(2*n))%history.size()];
  const float mean=std::isfinite(point.mean)?std::clamp(point.mean,-1.f,1.f):0.f;
  const float input=std::isfinite(point.rms)?std::clamp(point.rms,0.f,1.f):0.f;
  const float excursion=(std::isfinite(point.minimum)&&std::isfinite(point.maximum))
      ?std::clamp((point.maximum-point.minimum)*.5f,0.f,1.f):0.f;
  const float wave=std::sin(u*12.8f-motionTime)*(.035f+.07f*level)
                  +std::sin(u*24+motionTime*.6f)*.025f*level;
  centres[n]=height*(.44f+edge*(wave+.10f*mean));
  amplitudes[n]=height*edge*(.012f*level+.13f*(.65f*input+.35f*excursion)+.09f*wavePulse(u));
 }

 // Two bands trace the recent audio amplitude beneath the point cloud.
 if(level+impact>.002f) {
  for(int band=0;band<2;++band) {
   const float spread=band==0?2.9f:1.f;
   juce::Path ribbon;
   ribbon.preallocateSpace(6*ribbonSamples+8);
   for(int n=0;n<ribbonSamples;++n) {
    const auto p=wavePoint(float(n)/(ribbonSamples-1),centres[n]-spread*amplitudes[n]);
    if(n==0)ribbon.startNewSubPath(p);else ribbon.lineTo(p);
   }
   for(int n=ribbonSamples-1;n>=0;--n)
    ribbon.lineTo(wavePoint(float(n)/(ribbonSamples-1),centres[n]+spread*amplitudes[n]));
   ribbon.closeSubPath();
   const float alpha=band==0?.11f:.22f;
   juce::ColourGradient glow(accent.withAlpha(level*alpha*.15f),0,height*.44f,
                             secondary.withAlpha(level*alpha*.15f),width,height*.44f,false);
   glow.addColour(.48,accent.interpolatedWith(secondary,.45f).withAlpha(
      std::clamp(alpha*(level+impact),0.f,1.f)));
   g.setGradientFill(glow);
   g.fillPath(ribbon);
  }
 }

 for(size_t n=0;n<flow.size();++n) {
  const auto& mote=flow[n];
  const bool ribbon=n%3!=0;
  const float pulse=wavePulse(mote.u);
  const int index=std::clamp(int(mote.u*(ribbonSamples-1)),0,ribbonSamples-1);
  auto position=wavePoint(mote.u,centres[index]);
  position.x+=std::sin(motionTime+mote.seed)*(3.f*level+8.f*pulse);
  position.y+=mote.y*mote.edge*(.15f*height+swell*.24f*height)*(ribbon?.22f:.82f);
  position.y+=std::sin(mote.u*20-motionTime+mote.seed)*mote.depth*(4+8*impact);
  const float brightness=(.22f+.78f*mote.depth)*mote.edge*std::clamp(level+pulse,0.f,1.f);
  if(brightness<.008f)continue;
  const float size=(.85f+mote.radius*.75f)*(1.f+.8f*level+1.15f*pulse);
  const auto colour=accent.interpolatedWith(secondary,mote.depth>.64f?.85f:.16f);
  g.setColour(colour.interpolatedWith(juce::Colours::white,ribbon?.67f:.22f)
                  .withAlpha(std::clamp(brightness*(ribbon?1.2f:.68f),0.f,1.f)));
  const auto dot=juce::Rectangle<float>(size,size).withCentre(position);
  if(size>3.f)g.fillEllipse(dot);else g.fillRect(dot);
 }

 visible=0;
 if(effectEnabled)for(const auto& particle:model.particles) {
  const float light=particle.active?grainLight(particle.metadata,model.envelope(particle)):0.f;
  if(particle.trailEnergy>.009f) {
   const auto colour=particle.trailColour.interpolatedWith(secondary,.28f);
   const float tail=particle.trailEnergy;
   g.setGradientFill(juce::ColourGradient(colour.withAlpha(tail*.57f),particle.trailOrigin,
                                        colour.withAlpha(0.f),particle.trailEnd,false));
   g.drawLine({particle.trailOrigin,particle.trailEnd},1.2f+tail*3.5f);
   if(light<.06f)
    drawBloom(g,bloom,particle.trailOrigin,11.f+tail*17.f,colour.withAlpha(tail*.7f));
  }
  if(light<.008f)continue;
  ++visible;
  const auto centre=juce::Point<float>(particle.x,particle.y);
  const auto colour=juce::Colour(particle.metadata.colour).interpolatedWith(secondary,.24f);
  const float radius=2.2f+6.4f*light+3.f*light*impact;
  drawBloom(g,bloom,centre,13.f+radius*4.2f,colour.withAlpha(.78f*light));
  g.setColour(colour.withAlpha(.32f*light));
  g.drawEllipse(juce::Rectangle<float>(radius*3.5f,radius*3.5f).withCentre(centre),1.1f);
  g.setColour(colour.brighter(.3f).withAlpha(light));
  g.fillEllipse(juce::Rectangle<float>(radius*1.8f,radius*1.8f).withCentre(centre));
  g.setColour(juce::Colours::white.withAlpha(std::min(1.f,light*1.6f)));
  g.fillEllipse(juce::Rectangle<float>(radius*.85f,radius*.85f).withCentre(centre));
 }
 lastPaintMs=juce::Time::getMillisecondCounterHiRes()-began;
}
}
