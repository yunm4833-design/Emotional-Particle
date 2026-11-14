#include "Plugin.h"
#include "Visual/GranularVisualizer.h"
#include <iostream>
int main(int argc,char** argv){
 const juce::File outDir=argc>1?juce::File(juce::String(argv[1])):juce::File::getCurrentWorkingDirectory();outDir.createDirectory();
 juce::ScopedJuceInitialiser_GUI init;EmotionalProcessor p;p.prepareToPlay(48000,480);
 juce::AudioBuffer<float> b(2,480);juce::MidiBuffer midi;
 std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
 int block=0;
 auto audio=[&]{for(int n=0;n<480;++n){float t=float(block*480+n)/48000;float s=.30f*std::sin(juce::MathConstants<float>::twoPi*110*t)+.1f*std::sin(juce::MathConstants<float>::twoPi*441*t);b.setSample(0,n,s);b.setSample(1,n,s*.8f);}p.processBlock(b,midi);++block;};
 e->createComponentSnapshot(e->getLocalBounds());
 for(int i=0;i<650;++i){audio();juce::Timer::callPendingTimersSynchronously();juce::Thread::sleep(10);}
 audio();for(int i=0;i<e->getNumChildComponents();++i)if(auto* v=dynamic_cast<granular::GranularVisualizer*>(e->getChildComponent(i)))v->refreshNow();
 auto im=e->createComponentSnapshot(e->getLocalBounds());
 juce::FileOutputStream out(outDir.getChildFile("preview.png"));out.setPosition(0);out.truncate();
 if(!juce::PNGImageFormat().writeImageToStream(im,out))return 1;
 for(int i=0;i<e->getNumChildComponents();++i)if(auto* v=dynamic_cast<granular::GranularVisualizer*>(e->getChildComponent(i))){std::cout<<"Visible real grains: "<<v->visibleGrainCount()<<"; last visualizer paint "<<v->lastPaintMilliseconds()<<" ms\n";if(v->visibleGrainCount()==0)return 2;}
 for(int i=0;i<e->getNumChildComponents();++i)if(auto* button=dynamic_cast<juce::TextButton*>(e->getChildComponent(i)))if(button->getButtonText().startsWith("SOUND DETAILS"))button->onClick();
 if(e->getHeight()!=1010)return 3;
 for(int i=0;i<e->getNumChildComponents();++i)if(auto* slider=dynamic_cast<juce::Slider*>(e->getChildComponent(i)))if(slider->getTitle()=="PITCH"){slider->setValue(5,juce::sendNotificationSync);if(std::abs(p.state.getRawParameterValue("pitch")->load()-5)>.001)return 4;}
 for(int n=0;n<20;++n){audio();juce::Timer::callPendingTimersSynchronously();juce::Thread::sleep(10);}
 auto expanded=e->createComponentSnapshot(e->getLocalBounds());juce::FileOutputStream detailOut(outDir.getChildFile("preview-details.png"));detailOut.setPosition(0);detailOut.truncate();if(!juce::PNGImageFormat().writeImageToStream(expanded,detailOut))return 5;
 for(int i=0;i<e->getNumChildComponents();++i)if(auto* button=dynamic_cast<juce::TextButton*>(e->getChildComponent(i)))if(button->getButtonText()=="RESET DETAILS")button->onClick();
 if(std::abs(p.state.getRawParameterValue("pitch")->load())>.001f)return 6;
 std::cout<<"PASS: detail panel expansion, live UI pitch binding and Reset Details\n";
 return 0;
}
