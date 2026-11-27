#include "Plugin.h"
#include "Visual/GranularVisualizer.h"
#include <iostream>

namespace {
class FixedViewport final : public juce::Component {
public:
 int editorBoundsChanges=0;
 void childBoundsChanged(juce::Component*) override { ++editorBoundsChanges; }
};

bool checkInlineAuDetails(const juce::File& outDir) {
 juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_AudioUnit);
 EmotionalProcessor processor;
 juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);

 for(const float scale:{.5f,.75f,1.25f}) {
  const auto fail=[scale](const char* reason) {
   std::cerr<<"AU detail test at "<<juce::roundToInt(scale*100)<<"%: "<<reason<<'\n';
   return false;
  };
  processor.setEditorScale(scale);
  FixedViewport host;
  std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
  host.setSize(editor->getWidth(),editor->getHeight());
  host.setVisible(true);
  host.addAndMakeVisible(*editor);
  auto* canvas=editor->getChildComponent(0);
  if(canvas==nullptr)return fail("Missing editor canvas");

  juce::TextButton* details=nullptr;
  juce::TextButton* shortcut=nullptr;
  juce::TextButton* reset=nullptr;
  juce::Slider* pitch=nullptr;
  for(auto* child:canvas->getChildren()) {
   if(auto* button=dynamic_cast<juce::TextButton*>(child)) {
    if(button->getTitle()=="Sound Details")details=button;
    if(button->getTitle()=="Sound Details shortcut")shortcut=button;
    if(button->getButtonText()=="RESET DETAILS")reset=button;
   }
   if(auto* slider=dynamic_cast<juce::Slider*>(child);slider&&slider->getTitle()=="PITCH")
    pitch=slider;
  }
  const auto hitTest=[&](juce::Component& control) {
   const auto centre=host.getLocalPoint(&control,control.getLocalBounds().getCentre());
   auto* hit=host.getComponentAt(centre);
   return hit==&control||control.isParentOf(hit);
  };
  if(!details||!shortcut||!reset||!pitch||!hitTest(*details)||!hitTest(*shortcut)||pitch->isVisible())
   return fail("Toggle or initial hidden state invalid");
  host.editorBoundsChanges=0;
  details->onClick();
  if(host.editorBoundsChanges!=0||editor->getBounds()!=host.getLocalBounds()
     ||!details->getButtonText().endsWith("-")||!shortcut->getButtonText().endsWith("-"))
   return fail("Opening the panel resized the editor");

  int usableDetails=0;
  for(auto* child:canvas->getChildren()) {
   auto* slider=dynamic_cast<juce::Slider*>(child);
   if(!slider||!slider->isVisible()||slider->getComponentID()=="side"
      ||slider->getSliderStyle()!=juce::Slider::RotaryHorizontalVerticalDrag
      ||slider->getComponentID()=="intensity")
    continue;
   if(!host.getLocalBounds().contains(host.getLocalArea(slider,slider->getLocalBounds()))
      ||!hitTest(*slider))
    return fail("A detail control is clipped or covered");
   ++usableDetails;
  }
  if(usableDetails!=8||!hitTest(*reset))return fail("Detail controls or reset button missing");
  pitch->setValue(5,juce::sendNotificationSync);
  if(std::abs(processor.state.getRawParameterValue("pitch")->load()-5)>.001f)
   return fail("Pitch control did not update the processor");
  reset->onClick();
  if(std::abs(processor.state.getRawParameterValue("pitch")->load())>.001f)
   return fail("Reset Details did not reset pitch");

  if(scale==.75f) {
   auto image=editor->createComponentSnapshot(editor->getLocalBounds());
   juce::FileOutputStream preview(outDir.getChildFile("preview-au-details.png"));
   if(!preview.openedOk())return fail("Could not open preview output");
   preview.setPosition(0);preview.truncate();
   if(!juce::PNGImageFormat().writeImageToStream(image,preview))return fail("Could not write preview");
  }
  if(!hitTest(*details))return fail("Panel toggle is covered while open");
  shortcut->onClick();
  if(host.editorBoundsChanges!=0||pitch->isVisible()||reset->isVisible())
   return fail("Closing the panel left visible controls or resized the editor");
 }
 return true;
}
}

int main(int argc,char** argv){
 const juce::File outDir=argc>1?juce::File(juce::String(argv[1])):juce::File::getCurrentWorkingDirectory();outDir.createDirectory();
 juce::ScopedJuceInitialiser_GUI init;
 if(!checkInlineAuDetails(outDir)) {
  std::cerr<<"AU details failed in a fixed-size host viewport\n";
  return 16;
 }
 std::cout<<"PASS: eight AU detail controls and toggle are usable at 50%, 75%, and 125% without changing window bounds\n";
 EmotionalProcessor p;p.prepareToPlay(48000,480);
 juce::AudioBuffer<float> b(2,480);juce::MidiBuffer midi;
 std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());
 auto* canvas=e->getChildComponent(0);
 if(canvas==nullptr||!e->isResizable()||e->getWidth()!=1080)return 7;
 granular::GranularVisualizer* visualizer=nullptr;
 juce::TextButton* smaller=nullptr;
 juce::TextButton* larger=nullptr;
 for(auto* child:canvas->getChildren()) {
  if(auto* view=dynamic_cast<granular::GranularVisualizer*>(child))visualizer=view;
  if(child->getTitle()=="Decrease interface size")smaller=dynamic_cast<juce::TextButton*>(child);
  if(child->getTitle()=="Increase interface size")larger=dynamic_cast<juce::TextButton*>(child);
 }
 if(!smaller||!larger||!visualizer)return 8;
 smaller->onClick();
 if(e->getWidth()!=900||std::abs(canvas->getTransform().mat00-.625f)>.002f)return 9;
 larger->onClick();
 if(e->getWidth()!=1080)return 10;
 e->setBoundsConstrained({0,0,1152,648});
 if(e->getWidth()!=1152||std::abs(p.getEditorScale()-.8f)>.001f)return 11;
 e->setSize(1080,608);
 const auto snapshot=[&] {return e->createComponentSnapshot(e->getLocalBounds());};
 const auto waveBrightness=[](const juce::Image& image) {
  double total=0;
  for(int y=110;y<290;y+=3)for(int x=160;x<920;x+=3)
   total+=image.getPixelAt(x,y).getBrightness();
  return total;
 };
 const auto quiet=snapshot();
 for(int hit=0;hit<4;++hit) {
  for(int n=0;n<480;++n) {
   const float sample=.85f*std::sin(float(hit*480+n)*.025f);
   b.setSample(0,n,sample);b.setSample(1,n,sample*.8f);
  }
  p.processBlock(b,midi);
  juce::Thread::sleep(8);
  visualizer->refreshNow();
 }
 const auto attack=snapshot();
 if(waveBrightness(attack)<waveBrightness(quiet)*1.2) {
  std::cerr<<"The input pulse did not brighten the rendered wave\n";
  return 14;
 }
 juce::FileOutputStream pulseOut(outDir.getChildFile("preview-pulse.png"));
 pulseOut.setPosition(0);pulseOut.truncate();
 if(!juce::PNGImageFormat().writeImageToStream(attack,pulseOut))return 15;
 std::cout<<"PASS: rendered audio pulse brightens the wave\n";
 int block=0;
 auto audio=[&]{for(int n=0;n<480;++n){float t=float(block*480+n)/48000;float s=.30f*std::sin(juce::MathConstants<float>::twoPi*110*t)+.1f*std::sin(juce::MathConstants<float>::twoPi*441*t);b.setSample(0,n,s);b.setSample(1,n,s*.8f);}p.processBlock(b,midi);++block;};
 e->createComponentSnapshot(e->getLocalBounds());
 for(int i=0;i<650;++i){audio();juce::Timer::callPendingTimersSynchronously();juce::Thread::sleep(10);}
 audio();for(int i=0;i<canvas->getNumChildComponents();++i)if(auto* v=dynamic_cast<granular::GranularVisualizer*>(canvas->getChildComponent(i)))v->refreshNow();
 auto im=e->createComponentSnapshot(e->getLocalBounds());
 juce::FileOutputStream out(outDir.getChildFile("preview.png"));out.setPosition(0);out.truncate();
 if(!juce::PNGImageFormat().writeImageToStream(im,out))return 1;
 for(int i=0;i<canvas->getNumChildComponents();++i)if(auto* v=dynamic_cast<granular::GranularVisualizer*>(canvas->getChildComponent(i))){std::cout<<"Visible real grains: "<<v->visibleGrainCount()<<"; last visualizer paint "<<v->lastPaintMilliseconds()<<" ms\n";if(v->visibleGrainCount()==0)return 2;}
 for(int i=0;i<canvas->getNumChildComponents();++i)if(auto* button=dynamic_cast<juce::TextButton*>(canvas->getChildComponent(i)))if(button->getButtonText().startsWith("SOUND DETAILS"))button->onClick();
 if(e->getHeight()!=758||e->getWidth()!=1080)return 3;
 for(int i=0;i<canvas->getNumChildComponents();++i)if(auto* slider=dynamic_cast<juce::Slider*>(canvas->getChildComponent(i)))if(slider->getTitle()=="PITCH"){slider->setValue(5,juce::sendNotificationSync);if(std::abs(p.state.getRawParameterValue("pitch")->load()-5)>.001)return 4;}
 for(int n=0;n<20;++n){audio();juce::Timer::callPendingTimersSynchronously();juce::Thread::sleep(10);}
 auto expanded=e->createComponentSnapshot(e->getLocalBounds());juce::FileOutputStream detailOut(outDir.getChildFile("preview-details.png"));detailOut.setPosition(0);detailOut.truncate();if(!juce::PNGImageFormat().writeImageToStream(expanded,detailOut))return 5;
 for(int i=0;i<canvas->getNumChildComponents();++i)if(auto* button=dynamic_cast<juce::TextButton*>(canvas->getChildComponent(i)))if(button->getButtonText()=="RESET DETAILS")button->onClick();
 if(std::abs(p.state.getRawParameterValue("pitch")->load())>.001f)return 6;
 for(auto* child:canvas->getChildren()) {
  if(auto* slider=dynamic_cast<juce::Slider*>(child); slider&&slider->getTitle()=="MIX") {
   slider->setValue(0,juce::sendNotificationSync);
   if(p.state.getRawParameterValue("mix")->load()!=0.f||slider->getTextFromValue(0)!="0%")return 12;
  }
 }
 smaller->onClick();
 e.reset();
 e.reset(p.createEditor());
 if(e->getWidth()!=900||e->getHeight()!=506)return 13;
 std::cout<<"PASS: resize buttons, host bounds, persistent scale, detail expansion and parameter bindings\n";
 return 0;
}
