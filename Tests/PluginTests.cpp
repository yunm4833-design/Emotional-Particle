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
 restored.setStateInformation("bad",3);check(restored.getCurrentProgram()==1,"Malformed state");
 std::cout<<"PASS: host mono/stereo, sample rates/block sizes, programs and state\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
