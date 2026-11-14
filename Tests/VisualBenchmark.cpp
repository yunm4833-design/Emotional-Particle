#include "Visual/GranularVisualizer.h"
#include <iostream>
int main(){juce::ScopedJuceInitialiser_GUI init;granular::VisualTelemetry t;granular::GranularVisualizer v(t);v.setSize(1160,420);
 for(int i=0;i<128;++i){granular::GrainEvent e;e.kind=granular::GrainEventKind::Birth;e.id=i+1;e.slot=i;e.generation=1;e.birthSample=0;e.sampleStamp=24000;e.length=48000;e.sampleRate=48000;e.duration=1;e.pan=(i%16)/7.5f-1;e.pitch=(i/16)*6-24;e.sourceLevel=.3f;e.wet=1;e.colour=0xffedc591;e.readOffset=(i%13)/12.f;t.send(e);}
 t.audioClock=24000;for(int i=0;i<256;++i)t.send(granular::ScopePoint{.12f*std::sin(i*.1f),-.2f,.2f,.2f});for(int warm=0;warm<40;++warm){for(int n=0;n<32;++n)t.send(granular::ScopePoint{.12f*std::sin(n*.1f),-.2f,.2f,.2f});t.audioClock.fetch_add(1);v.refreshNow();juce::Thread::sleep(20);}
 juce::Image im(juce::Image::RGB,1160,420,true);juce::Graphics g(im);double sum=0,max=0;
 for(int warm=0;warm<10;++warm)v.paint(g);
 for(int n=0;n<120;++n){v.paint(g);auto ms=v.lastPaintMilliseconds();sum+=ms;max=std::max(max,ms);}
 std::cout<<"128-voice synthetic stress fixture: mean "<<sum/120<<" ms; max "<<max<<" ms; visible="<<v.visibleGrainCount()<<'\n';return v.visibleGrainCount()==128?0:1;}
