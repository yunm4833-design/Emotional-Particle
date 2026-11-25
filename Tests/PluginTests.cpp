#include "Plugin.h"
#include <iostream>
#include <stdexcept>
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{
 juce::ScopedJuceInitialiser_GUI init;EmotionalProcessor p;
 for(int channels:{1,2}){juce::AudioProcessor::BusesLayout layout;auto c=channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo();layout.inputBuses.add(c);layout.outputBuses.add(c);check(p.setBusesLayout(layout),"Bus layout");
 for(double rate:{44100.,48000.,96000.}){p.prepareToPlay(rate,512);for(int size:{0,1,17,64,512,1024}){juce::AudioBuffer<float> b(channels,size);b.clear();juce::MidiBuffer midi;p.processBlock(b,midi);for(int c=0;c<channels;++c)for(int n=0;n<size;++n)check(std::isfinite(b.getSample(c,n)),"Invalid audio");}}}
 for(int i=0;i<8;++i){p.setCurrentProgram(i);check(p.getCurrentProgram()==i,"Program selection");}
 check(p.getName()=="Emotional Particle","Product rename");check(p.getNumPrograms()==8,"Eight moods");
 auto set=[&](const char* id,float x){auto* q=p.state.getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(x));};
 set("size",1.6f);set("pitch",-7);set("width",.4f);set("drive",12);set("reverse",.2f);
 p.setCurrentProgram(1);auto* v=p.state.getParameter("intensity");v->setValueNotifyingHost(v->convertTo0to1(.33f));
 juce::MemoryBlock block;p.getStateInformation(block);EmotionalProcessor restored;restored.setStateInformation(block.getData(),int(block.getSize()));check(restored.getCurrentProgram()==1,"Mood restore");check(std::abs(restored.state.getRawParameterValue("intensity")->load()-.33f)<.001,"Intensity restore");check(std::abs(restored.state.getRawParameterValue("size")->load()-1.6f)<.001,"Size restore");
 check(std::abs(restored.state.getRawParameterValue("pitch")->load()+7)<.001,"Pitch restore");
 check(std::abs(restored.state.getRawParameterValue("drive")->load()-12)<.001,"Drive restore");
 auto* bypass=p.state.getParameter("bypass");bypass->setValueNotifyingHost(1);p.prepareToPlay(48000,64);juce::AudioBuffer<float> dry(2,64);for(int c=0;c<2;++c)for(int n=0;n<64;++n)dry.setSample(c,n,.25f);juce::MidiBuffer midi;p.processBlock(dry,midi);check(dry.getSample(0,63)==.25f,"Unity bypass");
 dry.setSample(0,0,std::numeric_limits<float>::quiet_NaN());dry.setSample(1,0,std::numeric_limits<float>::infinity());p.processBlock(dry,midi);check(std::isfinite(dry.getSample(0,0))&&std::isfinite(dry.getSample(1,0)),"Finite bypass input recovery");
 bypass->setValueNotifyingHost(0);p.prepareToPlay(48000,64);
 for(int block=0;block<32;++block){for(int c=0;c<2;++c)for(int n=0;n<64;++n)dry.setSample(c,n,.25f);p.processBlockBypassed(dry,midi);}
 check(dry.getSample(0,63)==.25f&&dry.getSample(1,63)==.25f,"Host callback unity bypass");
 auto* mix=p.state.getParameter("mix");
 check(mix->getText(0.f,32)=="0%"&&mix->getText(1.f,32)=="100%","Mix endpoint display");
 set("output",0);
 set("mix",1);
 p.prepareToPlay(48000,64);
 for(int block=0;block<1000;++block) {
  for(int n=0;n<64;++n) {
   const float input=.25f*std::sin(float(block*64+n)*.04f);
   dry.setSample(0,n,input);
   dry.setSample(1,n,-input);
  }
  p.processBlock(dry,midi);
 }
 check(p.voices.load()>0,"A live grain is required for the zero-mix regression");
 set("freeze",1);
 set("mix",0);
 p.setCurrentProgram(2);
 set("intensity",1);
 for(int block=0;block<48;++block) {
  for(int n=0;n<64;++n) {
   dry.setSample(0,n,.125f);
   dry.setSample(1,n,-.25f);
  }
  p.processBlock(dry,midi);
  if(block>=19)for(int n=0;n<64;++n)
   check(dry.getSample(0,n)==.125f&&dry.getSample(1,n)==-.25f,"Wet signal leaked at zero mix");
 }
 p.setEditorScale(.5f);
 p.getStateInformation(block);
 restored.setStateInformation(block.getData(),int(block.getSize()));
 check(restored.state.getRawParameterValue("mix")->load()==0.f&&restored.getEditorScale()==.5f,
       "New zero mix and editor scale survive state restore");
 auto oldState=p.state.copyState();
 oldState.removeProperty("absoluteMix",nullptr);
 oldState.getChildWithProperty("id","mood").setProperty("value",4,nullptr);
 oldState.getChildWithProperty("id","intensity").setProperty("value",.5f,nullptr);
 oldState.getChildWithProperty("id","mix").setProperty("value",-.2f,nullptr);
 juce::AudioProcessor::copyXmlToBinary(*oldState.createXml(),block);
 restored.setStateInformation(block.getData(),int(block.getSize()));
 check(std::abs(restored.state.getRawParameterValue("mix")->load()-.26f)<.0005f,
       "Legacy wet offset must migrate to its actual wet ratio");
 restored.setStateInformation("bad",3);check(restored.getCurrentProgram()==4,"Malformed state");
 std::cout<<"PASS: host mono/stereo, sample rates/block sizes, programs and state\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
