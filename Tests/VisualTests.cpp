#include "Visual/GranularVisualizer.h"
#include "DSP/MoodPresetManager.h"
#include <thread>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
static thread_local bool forbid=false;
void* operator new(std::size_t n){if(forbid)std::abort();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
#define CHECK(x) do{if(!(x))throw std::runtime_error("Failure at line "+std::to_string(__LINE__));}while(false)
int main(){try{
 using namespace granular;
 SpscFifo<std::uint64_t,31> fifo;for(std::uint64_t n=0;n<31;++n)CHECK(fifo.push(n));CHECK(!fifo.push(33));for(std::uint64_t n=0;n<31;++n){std::uint64_t v;CHECK(fifo.pop(v)&&v==n);}std::uint64_t v;CHECK(!fifo.pop(v));
 std::atomic<bool> good{true};
 std::thread producer([&]{forbid=true;for(std::uint64_t n=0;n<500000;++n)while(!fifo.push(n))std::this_thread::yield();forbid=false;});
 std::thread consumer([&]{forbid=true;for(std::uint64_t n=0;n<500000;++n){std::uint64_t value;while(!fifo.pop(value))std::this_thread::yield();if(value!=n)good=false;}forbid=false;});producer.join();consumer.join();CHECK(good.load());
 GranularProcessor observed,plain;VisualTelemetry t;observed.attachVisualTelemetry(&t);observed.prepare(48000);plain.prepare(48000);auto f=MoodPresetManager::targetFor(2,1,false);GrainEvent e;int births=0,updates=0,ends=0;ScopePoint point;
 for(int n=0;n<48000*5;++n){float x=.3f*std::sin(n*.035f);forbid=true;auto a=observed.processSmoothedSample(x,x*.7f,f);auto b=plain.processSmoothedSample(x,x*.7f,f);forbid=false;CHECK(a==b);
  while(t.grains.pop(e)){CHECK(e.slot<128);if(e.kind==GrainEventKind::Reset)continue;CHECK(e.pan>=-1&&e.pan<=1&&e.pitch>=-24&&e.pitch<=24);CHECK(e.duration>=.001&&e.duration<=.5);CHECK(e.envelopeGain>=0&&e.envelopeGain<=1.00001f);CHECK(e.envelopePhase>=0&&e.envelopePhase<=1);CHECK(e.colour==0xffedc591);births+=e.kind==GrainEventKind::Birth;updates+=e.kind==GrainEventKind::Update;ends+=e.kind==GrainEventKind::End;}
  while(t.scope.pop(point))CHECK(std::isfinite(point.rms)&&point.rms>=0);
 }
 CHECK(births>100&&updates>births&&ends>100&&t.droppedGrains.load()==0);
 GrainVisualModel model;
 for(int n=0;n<800;++n)observed.processSmoothedSample(.2f,.1f,f);t.audioClock.store(observed.getSampleClock());model.ingest(t);
 for(int block=0;block<200;++block){for(int n=0;n<128;++n)observed.processSmoothedSample(.2f,.1f,f);t.audioClock.store(observed.getSampleClock());model.ingest(t);CHECK(model.activeCount()==observed.getActiveGrainCount());}
 // Closed UI: overflow is bounded. Fresh full updates restore all current voices.
 for(int n=0;n<48000*10;++n)observed.processSmoothedSample(.2f,.1f,f);CHECK(t.droppedGrains.load()>0);t.audioClock.store(observed.getSampleClock());model.ingest(t);
 for(int n=0;n<800;++n)observed.processSmoothedSample(.2f,.1f,f);t.audioClock.store(observed.getSampleClock());model.ingest(t);CHECK(model.activeCount()==observed.getActiveGrainCount());
 auto clock=observed.getSampleClock();observed.prepare(96000);CHECK(observed.getSampleClock()==clock);model.ingest(t);CHECK(model.activeCount()==0);
 // Late End for a previous generation of a slot cannot remove its new voice.
 GrainVisualModel fixture;VisualTelemetry events;GrainEvent a;a.kind=GrainEventKind::Update;a.generation=1;a.id=2;a.slot=0;a.birthSample=0;a.length=24000;a.sampleStamp=100;a.duration=.5f;a.sourceLevel=.1f;a.wet=1;
 events.send(a);a.id=1;a.kind=GrainEventKind::End;events.send(a);events.audioClock=100;fixture.ingest(events);CHECK(fixture.activeCount()==1);
 events.audioClock=24001;fixture.ingest(events);CHECK(fixture.activeCount()==0);
 // A complete short grain can pass between two UI refreshes. Keep its peak
 // for the visual tail while still reporting zero active DSP grains.
 GrainEvent brief;
 brief.kind=GrainEventKind::Update;brief.generation=1;brief.id=3;brief.slot=1;
 brief.sampleRate=48000;brief.birthSample=24001;brief.sampleStamp=24121;
 brief.length=240;brief.envelopeGain=1;brief.sourceLevel=.2f;brief.wet=.5f;
 events.send(brief);
 brief.kind=GrainEventKind::End;brief.sampleStamp=24240;events.send(brief);
 events.audioClock=24300;fixture.ingest(events);
 CHECK(fixture.activeCount()==0&&fixture.particles[1].pendingPeak>.5f);
 events.audioClock=30000;fixture.ingest(events);
 CHECK(fixture.particles[1].pendingPeak==0);
 std::cout<<"PASS: FIFO overflow/wrap and 500k concurrent transfers; audio bit equality; per-grain metadata; model/voice count equality; closed-UI recovery; monotonic reset; stale End rejection; callback allocation guard\n";
 return 0;
}catch(const std::exception& e){forbid=false;std::cerr<<e.what()<<'\n';return 1;}}
