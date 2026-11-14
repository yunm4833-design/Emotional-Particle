#include "Plugin.h"
#include <thread>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <limits>
#include <atomic>
static void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
// Emulate a host that synchronously echoes a program update back to the plug-in.
struct EchoHost final:juce::AudioProcessorParameter::Listener {
 EmotionalProcessor& p;int calls=0;
 explicit EchoHost(EmotionalProcessor& v):p(v){}
 void parameterValueChanged(int,float) override {if(++calls<32)p.setCurrentProgram(p.getCurrentProgram());}
 void parameterGestureChanged(int,bool) override {}
};
int main(){try{
 juce::ScopedJuceInitialiser_GUI gui;EmotionalProcessor p;
 EchoHost host(p);auto* mood=p.state.getParameter("mood");mood->addListener(&host);
 p.setCurrentProgram(1);mood->removeListener(&host);
 require(host.calls==1,"Program echo re-entered host notifications");
 p.setCurrentProgram(-1);p.setCurrentProgram(8);require(p.getCurrentProgram()==1,"Invalid program changed state");
 p.prepareToPlay(48000,128);
 std::atomic<bool> done{false},good{true};std::atomic<int> blocks{0};
 std::thread audio([&]{juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;
  while(!done.load(std::memory_order_acquire)){
   for(int c=0;c<2;++c)for(int i=0;i<128;++i)b.setSample(c,i,.2f*std::sin(float(blocks.load()*128+i)*.03f));
   p.processBlock(b,midi);for(int c=0;c<2;++c)for(int i=0;i<128;++i)if(!std::isfinite(b.getSample(c,i)))good=false;
   ++blocks;
  }
 });
 int painted=0;
 for(int iteration=0;iteration<80;++iteration){
  std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
  for(int k=0;k<4;++k){
   p.setCurrentProgram((iteration+k)%8);
   for(const char* id:{"intensity","size","density","scatter","reverse","pitch","position","jitter","width","tone","mix","motion","drive","freeze","bypass"}){
    auto* q=p.state.getParameter(id);q->setValueNotifyingHost(((iteration+k)%3)*.5f);
   }
   juce::Timer::callPendingTimersSynchronously();
   auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
   if(!snapshot.isValid())good=false;++painted;
  }
 }
 done.store(true,std::memory_order_release);audio.join();
 require(good,"Nonfinite audio or failed UI snapshot during automation");require(blocks>0,"No concurrent audio blocks");
 // XML input is untrusted host data; malformed parameter values must be sanitised.
 auto bad=std::numeric_limits<float>::quiet_NaN();
 p.state.getRawParameterValue("mood")->store(bad);
 p.state.getRawParameterValue("output")->store(bad);
 p.state.getRawParameterValue("bypass")->store(bad);
 require(p.getCurrentProgram()>=0&&p.getCurrentProgram()<8,"Nonfinite mood lookup");
 juce::AudioBuffer<float> b(2,128);b.clear();juce::MidiBuffer midi;p.processBlock(b,midi);
 for(int c=0;c<2;++c)for(int i=0;i<128;++i)require(std::isfinite(b.getSample(c,i)),"Bad parameter contaminated output");
 std::cout<<"PASS: synchronous host echo; "<<blocks<<" concurrent audio blocks; 80 editor lifecycles; "<<painted<<" painted automation states; nonfinite parameter recovery\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
