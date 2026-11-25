#include "Plugin.h"
#include "Visual/GranularVisualizer.h"
#include <algorithm>
#include <cmath>
namespace {
const char* ids[]={"mood","intensity","freeze","output","size","density","scatter","reverse","pitch","position","jitter","width","tone","mix","motion","drive","bypass"};
constexpr int mixIndex = 13;
constexpr float defaultMix = .595f; // Dreamy at 70% Intensity in 0.5.
constexpr float defaultScale = .75f, minScale = .5f, maxScale = 1.25f;
constexpr int canvasWidth = 1440, compactHeight = 810, expandedHeight = 1010;
const juce::Identifier mixSchema{"absoluteMix"}, editorScaleProperty{"editorScale"};

juce::String formatMix(double proportion) {
 const double percent = std::round(proportion * 1000.0) / 10.0;
 return juce::String(percent, percent == std::floor(percent) ? 0 : 1) + "%";
}

float outputGainFor(float decibels) noexcept {
 // The 0.01 dB interval can round the top step slightly below 0 on FMA targets.
 if(decibels>=-.005f)return 1.f;
 return juce::Decibels::decibelsToGain(decibels);
}

float clampScale(float value) noexcept {
 return std::isfinite(value) ? std::clamp(value, minScale, maxScale) : defaultScale;
}
}
juce::AudioProcessorValueTreeState::ParameterLayout EmotionalProcessor::layout(){
 juce::AudioProcessorValueTreeState::ParameterLayout p;
 juce::StringArray names;for(auto& mood:granular::MoodPresetManager::presets)names.add(mood.name);
 p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mood",1},"Mood",names,2));
 p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"intensity",1},"Intensity",juce::NormalisableRange<float>(0,1,.001f),.7f));
 p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"freeze",1},"Freeze",false));
 p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output",1},"Output dB",juce::NormalisableRange<float>(-24,0,.01f),-6));
 const char* labels[]={"Grain size scale","Density scale","Scatter scale","Reverse offset","Pitch shift","Position offset","Pitch jitter scale","Stereo width scale","Tone scale","Mix","Motion scale","Wet drive dB"};
 const float lo[]={.25f,.25f,0,-.5f,-24,-.5f,0,0,.25f,0,0,0};
 const float hi[]={2,2,2,.5f,24,.5f,4,1.5f,2,1,2,18};
 const float initial[]={1,1,1,0,0,0,1,1,1,defaultMix,1,0};
 for(int n=0;n<12;++n) {
  juce::AudioParameterFloatAttributes attributes;
  if(n+4==mixIndex) {
   attributes=attributes.withStringFromValueFunction([](float value,int length) {
    const auto text=formatMix(value);
    return length>0?text.substring(0,length):text;
   }).withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue() / 100.f; });
  }
  p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ids[n+4],1},labels[n],
       juce::NormalisableRange<float>(lo[n],hi[n],.001f),initial[n],attributes));
 }
 p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));
 return p;
}
EmotionalProcessor::EmotionalProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"SporeMoodState",layout()){
 for(size_t n=0;n<raw.size();++n)raw[n]=state.getRawParameterValue(ids[n]);
 engine.attachVisualTelemetry(&visualTelemetry);
}
float EmotionalProcessor::parameter(int n) const noexcept {
 constexpr float low[]={0,0,0,-24,.25f,.25f,0,-.5f,-24,-.5f,0,0,.25f,0,0,0,0};
 constexpr float high[]={7,1,1,0,2,2,2,.5f,24,.5f,4,1.5f,2,1,2,18,1};
 constexpr float initial[]={2,.7f,0,-6,1,1,1,0,0,0,1,1,1,defaultMix,1,0,0};
 const float v=raw[size_t(n)]->load(std::memory_order_relaxed);
 return std::isfinite(v)?std::clamp(v,low[n],high[n]):initial[n];
}
int EmotionalProcessor::getCurrentProgram(){return int(std::lround(parameter(0)));}
const juce::String EmotionalProcessor::getProgramName(int n){return granular::MoodPresetManager::presets[std::clamp(n,0,7)].name;}
void EmotionalProcessor::setCurrentProgram(int n){if(n<0||n>7||n==getCurrentProgram())return;auto* p=state.getParameter("mood");p->setValueNotifyingHost(p->convertTo0to1(float(n)));}
void EmotionalProcessor::prepareToPlay(double sr,int){
 if(!std::isfinite(sr)||sr<8000||sr>384000)sr=48000;
 engine.prepare(sr,4);moods.prepare(sr,getCurrentProgram(),parameter(1),parameter(2)>.5f,details());
 bypassBlend.reset(sr,.03);bypassBlend.setCurrentAndTargetValue(parameter(16));
 wetMix.reset(sr,.025);
 wetMix.setCurrentAndTargetValue(parameter(mixIndex));
 outputGain.reset(sr,.03);outputGain.setCurrentAndTargetValue(outputGainFor(parameter(3)));meter=0;activity=0;voices=0;
}
bool EmotionalProcessor::isBusesLayoutSupported(const BusesLayout& b) const{
 auto c=b.getMainOutputChannelSet();return (c==juce::AudioChannelSet::mono()||c==juce::AudioChannelSet::stereo())&&c==b.getMainInputChannelSet();
}
void EmotionalProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){process(audio,midi,false);}
void EmotionalProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){process(audio,midi,true);}
void EmotionalProcessor::process(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi,bool hostBypassed){
 juce::ScopedNoDenormals noDenormals;midi.clear();
 if(audio.getNumChannels()<1||audio.getNumChannels()>2)return;
 const int mood=getCurrentProgram();
 engine.setVisualContext(granular::MoodPresetManager::presets[mood].accent,mood);
 moods.setTarget(mood,parameter(1),parameter(2)>.5f,details());
 bypassBlend.setTargetValue(hostBypassed?1.f:parameter(16));
 wetMix.setTargetValue(parameter(mixIndex));
 outputGain.setTargetValue(outputGainFor(parameter(3)));
 auto* l=audio.getWritePointer(0);auto* r=audio.getNumChannels()==2?audio.getWritePointer(1):nullptr;
 float peak=0;double energy=0;
 for(int n=0;n<audio.getNumSamples();++n){
  const float dryL=std::isfinite(l[n])?l[n]:0.f,dryR=r?(std::isfinite(r[n])?r[n]:0.f):dryL;
  auto frame=moods.next();
  frame[granular::GranularProcessor::Mix]=std::clamp(wetMix.getNextValue(),0.f,1.f);
  auto v=engine.processSmoothedSample(dryL,dryR,frame);
  const float gain=outputGain.getNextValue(),bypass=bypassBlend.getNextValue();v[0]=v[0]*gain*(1-bypass)+dryL*bypass;v[1]=v[1]*gain*(1-bypass)+dryR*bypass;
  l[n]=r?v[0]:.5f*(v[0]+v[1]);if(r)r[n]=v[1];
  peak=std::max(peak,std::max(std::abs(v[0]),std::abs(v[1])));energy+=double(v[0])*v[0]+double(v[1])*v[1];
 }
 visualTelemetry.audioClock.store(engine.getSampleClock(),std::memory_order_release);
 meter.store(peak,std::memory_order_relaxed);activity.store(audio.getNumSamples()>0?float(std::sqrt(energy/(2*audio.getNumSamples()))):0,std::memory_order_relaxed);voices.store(engine.getActiveGrainCount(),std::memory_order_relaxed);
}
void EmotionalProcessor::setEditorScale(float value) noexcept {
 editorScale.store(clampScale(value),std::memory_order_relaxed);
}
void EmotionalProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto snapshot=state.copyState();
 snapshot.setProperty(mixSchema,true,nullptr);
 snapshot.setProperty(editorScaleProperty,getEditorScale(),nullptr);
 copyXmlToBinary(*snapshot.createXml(),dest);
}
void EmotionalProcessor::setStateInformation(const void* data,int size) {
 if(data==nullptr||size<8||size>8*1024*1024)return;
 const auto xml=getXmlFromBinary(data,size);
 if(!xml||!xml->hasTagName(state.state.getType()))return;
 auto snapshot=juce::ValueTree::fromXml(*xml);
 if(!snapshot.hasProperty(mixSchema)) {
  const auto read=[&](const char* id,float fallback,float min,float max) {
   const auto entry=snapshot.getChildWithProperty("id",id);
   const auto value=entry.isValid()?static_cast<float>(entry.getProperty("value",fallback)):fallback;
   return std::isfinite(value)?std::clamp(value,min,max):fallback;
  };
  const int mood=int(std::lround(read("mood",2.f,0.f,7.f)));
  const float intensity=read("intensity",.7f,0.f,1.f);
  granular::DetailParameters previous;
  previous.mixBias=read("mix",0.f,-.5f,.5f);
  const auto frame=granular::MoodPresetManager::targetFor(mood,intensity,false,previous);
  auto mix=snapshot.getChildWithProperty("id",ids[mixIndex]);
  if(!mix.isValid()) {
   mix=juce::ValueTree("PARAM");
   mix.setProperty("id",ids[mixIndex],nullptr);
   snapshot.appendChild(mix,nullptr);
  }
  mix.setProperty("value",frame[granular::GranularProcessor::Mix],nullptr);
  snapshot.setProperty(mixSchema,true,nullptr);
 }
 setEditorScale(static_cast<float>(snapshot.getProperty(editorScaleProperty,defaultScale)));
 state.replaceState(snapshot);
}
granular::DetailParameters EmotionalProcessor::details() const noexcept {
 granular::DetailParameters d;float* values[]={&d.sizeScale,&d.densityScale,&d.scatterScale,&d.reverseBias,&d.pitchShift,&d.positionBias,&d.jitterScale,&d.widthScale,&d.toneScale,nullptr,&d.motionScale,&d.driveDb};
 for(int n=0;n<12;++n)if(values[n]!=nullptr)*values[n]=parameter(n+4);return d;
}

#include "UI/Typeface.h"
#include "UI/RotaryRing.h"
namespace {
struct EmotionalLook final:juce::LookAndFeel_V4 {
 juce::Colour accent{0xffa8eac8},secondary{0xffe2c68d};
 juce::Font getLabelFont(juce::Label& label) override{auto font=label.getFont();font.setHeight(std::max(17.f,font.getHeight()));return font;}
 juce::Font getTextButtonFont(juce::TextButton&,int height) override{return emotional::font(height>40?20.f:16.f,.015f);}
 juce::Font getSliderPopupFont(juce::Slider&) override{return emotional::font(19);}
 void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float value,float start,float end,juce::Slider& s) override {
  bool macro=s.getComponentID()=="intensity",side=s.getComponentID()=="side";float padding=macro?24.f:12.f;
  auto b=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(padding);if(!macro)b.removeFromBottom(18);
  float diameter=std::min(b.getWidth(),b.getHeight());b=juce::Rectangle<float>(diameter,diameter).withCentre(b.getCentre());if(side)b=juce::Rectangle<float>(80,80).withCentre({float(x)+width*.5f,float(y)+73});
  auto c=b.getCentre();float r=b.getWidth()*.5f;
  if(macro){g.setGradientFill(juce::ColourGradient(accent.withAlpha(.085f),c.x,c.y,accent.withAlpha(0.f),c.x+r+28,c.y,true));g.fillEllipse(b.expanded(28));g.setColour(accent.withAlpha(.12f));g.drawEllipse(b.expanded(12),1);g.drawEllipse(b.expanded(3),1);}
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff263b35),b.getX(),b.getY(),juce::Colour(0xff081411),b.getRight(),b.getBottom(),false));g.fillEllipse(b);
  g.setColour(accent.withAlpha(.18f));g.drawEllipse(b,1);
  float ring=r-(macro?9:4);value=std::isfinite(value)?std::clamp(value,0.f,1.f):0.f;const float arcEnd=start+(end-start)*value;
  g.setColour(accent.withAlpha(.10f));emotional::fillRing(g,c,ring,macro?7.f:2.5f,start,end);
  if(macro){g.setColour(accent.withAlpha(.12f));emotional::fillRing(g,c,ring,13,start,arcEnd);}
  g.setColour(accent);emotional::fillRing(g,c,ring,macro?5.f:2.2f,start,arcEnd);float angle=start+(end-start)*value;
  g.setColour(juce::Colour(0xffeef7eb));g.drawLine(c.x+std::sin(angle)*(r-(macro?45:20)),c.y-std::cos(angle)*(r-(macro?45:20)),c.x+std::sin(angle)*(r-(macro?22:8)),c.y-std::cos(angle)*(r-(macro?22:8)),macro?2.f:1.3f);
  if(macro){g.setFont(emotional::font(20,.10f));g.drawText("INTENSITY",b.translated(0,-7),juce::Justification::centred);g.setFont(emotional::font(34,.02f));g.drawText(juce::String(int(std::lround(s.getValue()*100)))+"%",b.translated(0,25),juce::Justification::centred);}
  else {g.setFont(emotional::font(20));g.setColour(juce::Colour(0xffe0ebe4));g.drawText(s.getTextFromValue(s.getValue()),x,side?y:y+height-22,width,26,juce::Justification::centred);}
 }
 void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool over,bool) override {g.setColour(accent.withAlpha(b.getToggleState()?.8f:over?.5f:.27f));g.drawRoundedRectangle(1,4,14,14,3,1);if(b.getToggleState()){g.fillRoundedRectangle(4,7,8,8,2);}g.setFont(emotional::font(17,.04f));g.setColour(juce::Colour(0xffd4e4da));g.drawText(b.getButtonText(),22,0,b.getWidth()-22,b.getHeight(),juce::Justification::centredLeft);}
 void drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour&,bool over,bool down) override {
  auto b=button.getLocalBounds().toFloat().reduced(1);if(button.getComponentID()=="power")return;
  if(button.getComponentID()=="textOnly"){g.setColour(accent.withAlpha(over?.1f:.02f));g.fillRoundedRectangle(b,10);return;}
  bool on=button.getToggleState();
  if(on){g.setColour(accent.withAlpha(.10f));g.fillRoundedRectangle(b.expanded(2),b.getHeight()*.5f);}
  g.setGradientFill(juce::ColourGradient(on?accent.brighter(.1f):juce::Colour(0xff243c34).withAlpha(over?.95f:.6f),0,0,on?accent.darker(.25f):juce::Colour(0xff0d1e18).withAlpha(.65f),0,b.getBottom(),false));g.fillRoundedRectangle(b,b.getHeight()*.5f);
  g.setColour(on?accent.brighter(.4f):accent.withAlpha(down?.6f:.32f));g.drawRoundedRectangle(b,b.getHeight()*.5f,on?1.2f:.8f);
 }
 void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool,bool) override {
  if(button.getComponentID()=="power"){
   auto b=button.getLocalBounds().toFloat().reduced(9);auto c=b.getCentre();float r=b.getWidth()*.5f;g.setColour(button.getToggleState()?accent.withAlpha(.2f):accent);
   emotional::fillRing(g,c,r,2,.5f,juce::MathConstants<float>::twoPi-.5f);g.drawLine(c.x,c.y-r-3,c.x,c.y-1,2);return;
  }
  g.setFont(getTextButtonFont(button,button.getHeight()));g.setColour(button.getToggleState()?juce::Colour(0xff092219):juce::Colour(0xffd5e7dc));g.drawText(button.getButtonText(),button.getLocalBounds(),juce::Justification::centred);
 }
};
}
class EmotionalEditor final:public juce::AudioProcessorEditor,private juce::Timer {
 EmotionalProcessor& processor;
 EmotionalLook look;
 juce::TooltipWindow tooltips{this,650};
 juce::Component canvas;
 granular::GranularVisualizer visualizer;
 juce::Slider intensity,trim;
 std::array<juce::Slider,12> detailSliders;
 std::array<juce::Label,12> detailLabels;
 std::array<juce::TextButton,8> buttons;
 juce::TextButton previous{"<"},next{">"},power,detailsButton{"SOUND DETAILS  +"},resetButton{"RESET DETAILS"};
 juce::TextButton smaller{"-"}, larger{"+"};
 juce::Label zoomLabel;
 juce::ToggleButton freeze{"FREEZE"};
 std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> intensityA,trimA;
 std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,12> detailA;
 std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeA,powerA;
 const std::array<int,8> order{0,2,3,1,4,5,6,7};
 std::array<juce::SmoothedValue<float>,9> colour;
 int selected=-1;
 bool expanded=false, applyingSize=false;
 float peak=0, canvasScale=defaultScale, appliedScale=defaultScale;
 juce::Colour background;
 juce::Colour componentColour(int n) const{return juce::Colour::fromFloatRGBA(colour[n].getCurrentValue(),colour[n+1].getCurrentValue(),colour[n+2].getCurrentValue(),1);}
 void selectMood(int n){auto* p=processor.state.getParameter("mood");p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(float(n)));p->endChangeGesture();}
 void updateTheme(bool initial){int m=processor.getCurrentProgram();if(m==selected)return;selected=m;const auto& p=granular::MoodPresetManager::presets[m];std::array<juce::Colour,3> c{juce::Colour(p.background),juce::Colour(p.accent),juce::Colour(p.secondary)};
  for(int n=0;n<9;++n){float v=n%3==0?c[n/3].getFloatRed():n%3==1?c[n/3].getFloatGreen():c[n/3].getFloatBlue();if(initial)colour[n].setCurrentAndTargetValue(v);else colour[n].setTargetValue(v);}
 }
 void timerCallback() override {
  const float savedScale=processor.getEditorScale();
  if(savedScale!=appliedScale)applySize(savedScale);
  updateTheme(false);for(auto& c:colour)c.getNextValue();background=componentColour(0);look.accent=componentColour(3);look.secondary=componentColour(6);
  for(int n=0;n<8;++n)buttons[n].setToggleState(order[n]==selected,juce::dontSendNotification);
  peak=std::max(processor.meter.load(),peak*.92f);trim.setColour(juce::Slider::thumbColourId,look.accent);trim.setColour(juce::Slider::trackColourId,look.accent);visualizer.setTheme(background,look.accent,look.secondary,selected);visualizer.setEffectEnabled(!power.getToggleState());
  for(auto& l:detailLabels)l.setColour(juce::Label::textColourId,look.accent.withAlpha(.95f));repaint();
 }
 int layoutHeight() const noexcept { return expanded?expandedHeight:compactHeight; }
 void applySize(float requestedScale) {
  const juce::ScopedValueSetter<bool> guard(applyingSize,true);
  const float scale=clampScale(requestedScale);
  const int height=layoutHeight();
  if(auto* constrainer=getConstrainer())
   constrainer->setFixedAspectRatio(double(canvasWidth)/height);
  setSize(juce::roundToInt(canvasWidth*scale),juce::roundToInt(height*scale));
  setResizeLimits(juce::roundToInt(canvasWidth*minScale),juce::roundToInt(height*minScale),
                  juce::roundToInt(canvasWidth*maxScale),juce::roundToInt(height*maxScale));
  getConstrainer()->setFixedAspectRatio(double(canvasWidth)/height);
  appliedScale=scale;
  processor.setEditorScale(scale);
  resized();
 }
 void setExpanded(bool on) {
  expanded=on;
  detailsButton.setButtonText(on?"SOUND DETAILS  -":"SOUND DETAILS  +");
  for(int n=4;n<12;++n) {
   detailSliders[n].setVisible(on);
   detailLabels[n].setVisible(on);
  }
  resetButton.setVisible(on);
  applySize(processor.getEditorScale());
 }

public:
 explicit EmotionalEditor(EmotionalProcessor& p):AudioProcessorEditor(p),processor(p),visualizer(p.visualTelemetry){
  for(auto& c:colour)c.reset(60,.8);updateTheme(true);background=componentColour(0);look.accent=componentColour(3);look.secondary=componentColour(6);setLookAndFeel(&look);
  addAndMakeVisible(canvas);
  canvas.addAndMakeVisible(visualizer);visualizer.setTheme(background,look.accent,look.secondary,selected);
  intensity.setComponentID("intensity");intensity.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);intensity.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);intensity.setDoubleClickReturnValue(true,.7);canvas.addAndMakeVisible(intensity);intensityA=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"intensity",intensity);
  trim.setSliderStyle(juce::Slider::LinearHorizontal);trim.setTextBoxStyle(juce::Slider::TextBoxRight,false,84,28);trim.setTextValueSuffix(" dB");trim.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);trim.setColour(juce::Slider::trackColourId,look.accent);canvas.addAndMakeVisible(trim);trimA=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"output",trim);
  const char* labels[]={"GRAIN","DENSITY","SCATTER","REVERSE","PITCH","POSITION","PITCH JITTER","WIDTH","TONE","MIX","MOTION","DRIVE"};
  const char* tips[]={"Scale the mood's grain length; double-click to reset","Scale the mood's average trigger rate","Scale time-position scatter","Add to the mood's reverse probability","Transpose grains in semitones","Offset the safe reading position","Scale pitch randomness","Scale stereo panning spread","Scale the mood's low-pass cutoff","Wet signal percentage, from dry to fully wet","Scale rhythm and tape drift movement","Add saturation to the wet signal"};
  const float neutral[]={1,1,1,0,0,0,1,1,1,defaultMix,1,0};
  for(int n=0;n<12;++n){auto& s=detailSliders[n];if(n<4)s.setComponentID("side");s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);s.setNumDecimalPlacesToDisplay(2);s.setDoubleClickReturnValue(true,neutral[n]);s.setTooltip(tips[n]);s.setTitle(labels[n]);s.setTextValueSuffix(n==4?" st":n==11?" dB":(n==3||n==5||n==9)?"":"x");canvas.addAndMakeVisible(s);detailA[n]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[n+4],s);
   detailLabels[n].setText(labels[n],juce::dontSendNotification);detailLabels[n].setFont(emotional::font(n<4?20.f:18.f,.04f));detailLabels[n].setJustificationType(juce::Justification::centred);canvas.addAndMakeVisible(detailLabels[n]);}
  detailSliders[0].textFromValueFunction=[this](double v){return juce::String(int(std::clamp(granular::MoodPresetManager::presets[processor.getCurrentProgram()].values[0]*float(v),1.f,500.f)))+" ms";};
  detailSliders[1].textFromValueFunction=[this](double v){return juce::String(int(std::clamp(granular::MoodPresetManager::presets[processor.getCurrentProgram()].values[1]*float(v),1.f,100.f)))+" Hz";};
  detailSliders[2].textFromValueFunction=[this](double v){return juce::String(int(std::clamp(granular::MoodPresetManager::presets[processor.getCurrentProgram()].values[4]*float(v),0.f,500.f)*float(intensity.getValue())))+" ms";};
  detailSliders[3].textFromValueFunction=[this](double v){return juce::String(int(std::clamp(granular::MoodPresetManager::presets[processor.getCurrentProgram()].values[11]+float(v),0.f,1.f)*100))+"%";};
  for(int n=0;n<4;++n)detailSliders[n].setTextValueSuffix("");
  for(int n=4;n<12;++n)detailSliders[n].textFromValueFunction=[n](double v){return n==9?formatMix(v):n==5?(v>0?"+":"")+juce::String(v*100,0)+"%":juce::String(v,n==4?1:2);};
  canvas.addAndMakeVisible(freeze);freezeA=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"freeze",freeze);
  power.setComponentID("power");power.setTitle("Bypass");power.setClickingTogglesState(true);power.setTooltip("Bypass effect");canvas.addAndMakeVisible(power);powerA=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bypass",power);
  for(int n=0;n<8;++n){buttons[n].setButtonText(granular::MoodPresetManager::presets[order[n]].name);buttons[n].onClick=[this,n]{selectMood(order[n]);};buttons[n].setToggleState(order[n]==selected,juce::dontSendNotification);canvas.addAndMakeVisible(buttons[n]);}
  for(auto* b:{&previous,&next,&detailsButton,&resetButton}){b->setComponentID("textOnly");canvas.addAndMakeVisible(*b);}
  previous.onClick=[this]{selectMood((processor.getCurrentProgram()+7)%8);};next.onClick=[this]{selectMood((processor.getCurrentProgram()+1)%8);};detailsButton.onClick=[this]{setExpanded(!expanded);};
  resetButton.onClick=[this]{const float neutral[]={1,1,1,0,0,0,1,1,1,defaultMix,1,0};for(int n=0;n<12;++n){auto* p=processor.state.getParameter(ids[n+4]);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(neutral[n]));p->endChangeGesture();}};
  for(auto* button:{&smaller,&larger}) {
   button->setComponentID("textOnly");
   canvas.addAndMakeVisible(*button);
  }
  smaller.setTitle("Decrease interface size");
  smaller.setTooltip("Decrease interface size");
  larger.setTitle("Increase interface size");
  larger.setTooltip("Increase interface size");
  smaller.onClick=[this]{applySize(appliedScale-.125f);};
  larger.onClick=[this]{applySize(appliedScale+.125f);};
  zoomLabel.setJustificationType(juce::Justification::centred);
  zoomLabel.setFont(emotional::font(17));
  zoomLabel.setTitle("Interface size");
  canvas.addAndMakeVisible(zoomLabel);
  peak=processor.meter.load();setExpanded(false);setResizable(true,true);startTimerHz(60);
 }
 ~EmotionalEditor() override{stopTimer();setLookAndFeel(nullptr);}
 void resized() override {
  if(getWidth()<=0||getHeight()<=0)return;
  canvasScale=std::min(float(getWidth())/canvasWidth,float(getHeight())/layoutHeight());
  canvas.setBounds(0,0,canvasWidth,layoutHeight());
  canvas.setTransform(juce::AffineTransform::scale(canvasScale));
  if(!applyingSize) {
   appliedScale=clampScale(float(getWidth())/canvasWidth);
   processor.setEditorScale(appliedScale);
  }
  zoomLabel.setText(juce::String(juce::roundToInt(appliedScale*100))+"%",juce::dontSendNotification);
  smaller.setEnabled(appliedScale>minScale);
  larger.setEnabled(appliedScale<maxScale);
  smaller.setBounds(1190,772,35,32);
  zoomLabel.setBounds(1226,772,64,32);
  larger.setBounds(1291,772,35,32);
  visualizer.setBounds(140,104,1160,420);intensity.setBounds(570,382,300,300);
  trim.setBounds(1080,71,250,28);freeze.setBounds(1180,105,130,28);power.setBounds(1368,31,45,45);
  previous.setBounds(505,40,36,33);next.setBounds(899,40,36,33);
  const int xs[]={32,32,1300,1300},ys[]={178,371,178,371};for(int n=0;n<4;++n){detailLabels[n].setBounds(xs[n],ys[n],108,24);detailSliders[n].setBounds(xs[n],ys[n]+25,108,125);}
  for(int n=0;n<8;++n)buttons[n].setBounds(40+n*172,722,160,50);
  detailsButton.setBounds(610,777,220,30);resetButton.setBounds(1170,826,210,30);
  for(int n=4;n<12;++n){int x=72+(n-4)*164;detailLabels[n].setBounds(x,855,140,22);detailSliders[n].setBounds(x+12,881,116,108);}
 }
 void paint(juce::Graphics& g) override {
  g.fillAll(background.darker(.55f));
  g.addTransform(juce::AffineTransform::scale(canvasScale));
  g.setGradientFill(juce::ColourGradient(background.brighter(.02f),720,260,background.darker(.65f),720,810,false));g.fillRect(0,0,canvasWidth,layoutHeight());
  g.setGradientFill(juce::ColourGradient(look.accent.withAlpha(.065f),720,360,look.accent.withAlpha(0.f),720,80,true));g.fillEllipse(250,70,940,580);
  g.setColour(look.accent.withAlpha(.20f));g.fillRoundedRectangle(35,32,8,44,3);g.setColour(look.accent);g.fillRoundedRectangle(38,34,2,40,1);
  g.setColour(juce::Colour(0xffedf7f0));g.setFont(emotional::font(32,.025f));g.drawText("EMOTIONAL PARTICLE",60,28,410,40,juce::Justification::centredLeft);
  g.setFont(emotional::font(16,.08f));g.setColour(look.accent.withAlpha(.90f));g.drawText("GRANULAR  /  MOOD ENGINE",61,73,380,20,juce::Justification::centredLeft);
  g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(.065f),505,35,look.accent.withAlpha(.025f),935,78,false));g.fillRoundedRectangle(505,35,430,43,18);g.setColour(look.accent.withAlpha(.25f));g.drawRoundedRectangle(505,35,430,43,18,.8f);
  g.setColour(juce::Colour(0xffe2f0e8));g.setFont(emotional::font(21,.025f));g.drawText(juce::String(granular::MoodPresetManager::presets[std::max(0,selected)].subtitle).toLowerCase(),550,35,340,43,juce::Justification::centred);
  g.setFont(emotional::font(16,.02f));g.setColour(look.accent.withAlpha(.7f));g.drawText("OUT",1030,42,40,18,juce::Justification::centredLeft);
  const float meterLevel=juce::jlimit(0.f,1.f,(juce::Decibels::gainToDecibels(peak,-60.f)+60.f)/60.f);
  for(int n=0;n<21;++n){g.setColour(n<meterLevel*21?look.accent:look.accent.withAlpha(.1f));g.fillRoundedRectangle(1082+n*10,43,4,15,1);}
  g.setColour(look.accent.withAlpha(.15f));g.drawLine(1344,32,1344,80,1);g.drawLine(26,357,134,357,.7f);g.drawLine(1304,357,1412,357,.7f);
  if(expanded){g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(.045f),28,821,look.accent.withAlpha(.02f),1412,994,false));g.fillRoundedRectangle(28,821,1384,178,16);g.setColour(look.accent.withAlpha(.13f));g.drawRoundedRectangle(28,821,1384,178,16,1);g.setColour(look.accent.withAlpha(.9f));g.setFont(emotional::font(17,.07f));g.drawText("SCULPT THE FEELING",59,828,320,20,juce::Justification::centredLeft);}
 }
};
juce::AudioProcessorEditor* EmotionalProcessor::createEditor(){return new EmotionalEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new EmotionalProcessor;}
