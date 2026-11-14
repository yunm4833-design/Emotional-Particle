#include "GranularVisualizer.h"
#include "../UI/Typeface.h"
#include "../DSP/GranularProcessor.h"
#include <algorithm>
#include <cmath>
namespace granular {
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
 }
 clock=std::max(clock,t.audioClock.load(std::memory_order_acquire));
 for(auto& p:particles)if(p.active && (clock<p.metadata.birthSample || clock-p.metadata.birthSample>=std::uint64_t(p.metadata.length)))p.active=false;
}
float GrainVisualModel::phase(const Particle& p) const noexcept {
 if(clock<=p.metadata.birthSample)return 0;
 return std::clamp(float(clock-1-p.metadata.birthSample)/float(std::max(1,p.metadata.length-1)),0.f,1.f);
}
float GrainVisualModel::envelope(const Particle& p) const noexcept {
 return GranularProcessor::windowEnvelope(phase(p),p.metadata.windowMorph,p.metadata.windowEdge);
}
int GrainVisualModel::activeCount() const noexcept {int n=0;for(auto& p:particles)n+=p.active;return n;}
GranularVisualizer::GranularVisualizer(VisualTelemetry& t):telemetry(t){
 setInterceptsMouseClicks(false,false);setOpaque(false);
 lastFrame=lastClockChange=juce::Time::getMillisecondCounterHiRes()*.001;
 for(int n=0;n<8;++n){motion[n].reset(60,.8);motion[n].setCurrentAndTargetValue(n==2?1.f:0.f);}
 juce::Random random(671928);
 for(auto& p:flow)p={random.nextFloat(),random.nextFloat()+random.nextFloat()+random.nextFloat()-1.5f,random.nextFloat()*6.2831853f,.55f+random.nextFloat()*1.5f,random.nextFloat()};
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
 ScopePoint point;float energy=0;int samples=0;
 for(int n=0;n<2048&&telemetry.scope.pop(point);++n){history[historyWrite++%history.size()]=point;energy+=point.rms;samples++;}
 const float targetRms=samples?energy/samples:0.f;
 rms+=(targetRms-rms)*(1-std::exp(-dt*12));
 for(auto& s:motion)s.getNextValue();
 const float anxious=motion[1].getCurrentValue(),calm=motion[3].getCurrentValue();
 constexpr float spring[]={14,114,20,10,18,24,8,100},drag[]={14,7,9,17,11,10,19,6};
 float stiffness=0,damping=0;for(int n=0;n<8;++n){const float weight=std::clamp(motion[n].getCurrentValue(),0.f,1.f);stiffness+=spring[n]*weight;damping+=drag[n]*weight;}
 const float width=float(getWidth()),height=float(getHeight());
 for(auto& p:model.particles)if(p.active){
  const auto& e=p.metadata;const float phase=model.phase(p);
  const float x=width*(.5f+.35f*e.pan+.22f*(.5f-e.readOffset));
  const float y=height*(.48f-.30f*e.pitch/24.f)-10*calm*phase;
  const float jitter=anxious*4*std::sin(time*130+float(e.id%31));
  if(!p.placed){p.x=x;p.y=y;p.vx=p.vy=0;p.placed=true;}
  p.vx+=((x+jitter-p.x)*stiffness-p.vx*damping)*dt;
  p.vy+=((y+jitter*.7f-p.y)*stiffness-p.vy*damping)*dt;
  p.x+=p.vx*dt;p.y+=p.vy*dt;
 }
 repaint();
}
void GranularVisualizer::paint(juce::Graphics& g){
 const double began=juce::Time::getMillisecondCounterHiRes();
 const auto w=float(getWidth()),h=float(getHeight());if(w<=1||h<=1)return;
 const float middle=h*.48f,level=std::clamp(rms*5.f,0.f,1.f);
 // Audio-reactive light-flow layer: many artistic motes, distinct from real voice nuclei.
 // The ribbon reads smoothed input amplitude history; no extra DSP voices are invented.
 const float slow=time*.22f;
 const auto centreAt=[&](float u,float depth){
  const int index=std::clamp(int(u*255),0,255);float audio=0;
  for(int k=-3;k<=3;++k)audio+=history[(historyWrite+unsigned(std::clamp(index+k,0,255)))%256].mean;
  audio/=7;
  const float envelope=std::pow(std::max(0.f,std::sin(juce::MathConstants<float>::pi*u)),.65f);
  return h*.43f+std::sin(u*12.8f-slow)*h*.105f*envelope
     +std::sin(u*24+slow*.6f+depth*2)*h*.043f*envelope
     +audio*h*.14f;
 };
 // Soft volume beneath the thin bright ribbons.
 for(int i=0;i<22;++i){float u=(i+.5f)/22,x=w*(.04f+.92f*u),y=centreAt(u,.5f);float radius=40+level*48;
  auto c=accent.interpolatedWith(secondary,u*.35f);g.setGradientFill(juce::ColourGradient(c.withAlpha(level*.07f),x,y,c.withAlpha(0.f),x+radius,y,true));g.fillEllipse(x-radius,y-radius,radius*2,radius*2);
 }
 std::array<float,256> centres;for(int n=0;n<256;++n)centres[n]=centreAt(n/255.f,.5f);
 int i=0;
 for(const auto& p:flow){
  const float u=p.u,edge=std::pow(std::max(0.f,std::sin(juce::MathConstants<float>::pi*u)),.7f);
  const bool ribbon=(i%3)!=0;
  const float band=ribbon?.20f:1.f;
  float x=w*(.035f+.93f*u)+std::sin(slow+p.seed)*level*4;
  float y=centres[std::clamp(int(u*255),0,255)]+p.y*h*.36f*edge*band*(.4f+level*.6f)
        +std::sin(u*20-slow+p.seed)*p.depth*8;
  const float brightness=(.25f+.75f*p.depth)*edge*level;
  const float size=(.8f+p.radius*.65f)*(1.f+.6f*level)*(ribbon?.85f:1.12f);
  auto c=accent.interpolatedWith(secondary,p.depth>.65f?.75f:.12f);
  if(i%61==0){float r=size*5;g.setGradientFill(juce::ColourGradient(c.withAlpha(brightness*.18f),x,y,c.withAlpha(0.f),x+r,y,true));g.fillEllipse(x-r,y-r,r*2,r*2);}
  g.setColour(c.interpolatedWith(juce::Colours::white,ribbon?.70f:.20f).withAlpha(std::clamp(brightness*(ribbon?1.25f:.90f),0.f,1.f)));
  if(size>2.5f)g.fillEllipse(x-size*.5f,y-size*.5f,size,size);else g.fillRect(juce::Rectangle<float>(x-size*.5f,y-size*.5f,size,size));++i;
 }
 // Large true-grain nuclei retain the metadata/voice lifecycle contract.
 visible=0;
 for(const auto& p:model.particles){if(!p.active || !effectEnabled)continue;float envelope=model.envelope(p);float audible=std::clamp(p.metadata.sourceLevel*9,0.f,1.f)*p.metadata.wet;
  float alpha=std::clamp(envelope*audible,0.f,1.f);if(alpha<.003f)continue;++visible;
  float radius=2.f+5.5f*std::sqrt(envelope);auto c=juce::Colour(p.metadata.colour).interpolatedWith(accent,.18f);
  g.setGradientFill(juce::ColourGradient(c.withAlpha(alpha*.22f),p.x,p.y,c.withAlpha(0.f),p.x+radius*4,p.y,true));g.fillEllipse(p.x-radius*4,p.y-radius*4,radius*8,radius*8);
  g.setColour(c.withAlpha(alpha*.12f));g.drawEllipse(p.x-radius*1.5f,p.y-radius*1.5f,radius*3,radius*3,.7f);
  g.setColour(c.withAlpha(alpha*.60f));g.fillEllipse(p.x-radius*.44f,p.y-radius*.44f,radius*.88f,radius*.88f);
  g.setColour(juce::Colours::white.withAlpha(alpha*.85f));g.fillEllipse(p.x-1.15f,p.y-1.15f,2.3f,2.3f);
 }
 lastPaintMs=juce::Time::getMillisecondCounterHiRes()-began;
}
}
