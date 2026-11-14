#include "DSP/GranularProcessor.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
#include <cmath>
#include <limits>
static bool allocationForbidden=false;
void* operator new(std::size_t n){if(allocationForbidden)std::abort();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
#define check(b) do {if(!(b))throw std::runtime_error("Test failed at line " + std::to_string(__LINE__));} while(false)
int main(){try{
 using namespace granular;
 static_assert(GranularProcessor::maxGrains>=64);
 for(float morph: {0.f,.5f,1.f,1.5f,2.f}) {
  check(GranularProcessor::envelope(0,morph)==0);
  check(GranularProcessor::envelope(1,morph)==0);
  for(int i=0;i<1001;++i){float v=GranularProcessor::envelope(i/1000.f,morph);check(v>=0&&v<=1.00001f);}
 }
 for(double sr:{8000.,44100.,48000.,96000.,192000.}){
  GranularProcessor a,b;Parameters p;p.dryWet=0;
  a.prepare(sr,4,p);b.prepare(sr,4,p);
  juce::AudioBuffer<float> x(2,257),y(2,257);
  for(int block=0;block<int(sr*7/257);++block){
   if(block==1)p.dryWet=1;
   if(block%101==0){p.grainMs=(block%202==0?500:1);p.pitchSemitones=(block%202==0?24:-24);p.densityHz=100;p.position=(block%202==0?1:0);p.timeSprayMs=500;p.pitchJitter=12;p.panJitter=1;p.windowMorph=(block%3);p.freeze=(block%303!=0);}
   for(int i=0;i<257;++i){float v=.4f*std::sin(float(block*257+i)*.023f);x.setSample(0,i,v);x.setSample(1,i,-v);y.setSample(0,i,v);y.setSample(1,i,-v);}
   allocationForbidden=true;a.processBlock(x,p);b.processBlock(y,p);allocationForbidden=false;
   for(int c=0;c<2;++c)for(int i=0;i<257;++i){check(std::isfinite(x.getSample(c,i)));check(x.getSample(c,i)==y.getSample(c,i));}
  }
 }
 // Block partition invariance and wet energy, including ring wrap (>4s).
 GranularProcessor a,b;Parameters p;p.dryWet=1;p.grainMs=50;p.densityHz=100;p.pitchJitter=12;
 a.prepare(48000,4,p);b.prepare(48000,4,p);
 juce::AudioBuffer<float> big(2,256),single(2,1);double energy=0;
 for(int block=0;block<1000;++block){
  for(int i=0;i<256;++i){float v=.3f*std::sin((block*256+i)*.031f);big.setSample(0,i,v);big.setSample(1,i,v);}
  a.processBlock(big,p);
  for(int i=0;i<256;++i){float v=.3f*std::sin((block*256+i)*.031f);single.setSample(0,0,v);single.setSample(1,0,v);b.processBlock(single,p);check(big.getSample(0,i)==single.getSample(0,0));energy+=big.getSample(0,i)*big.getSample(0,i);}
 }
 check(energy>1);
 // Feed identical audio during the freeze ramp, divergent audio after it settles.
 p.freeze=true;
 for(int block=0;block<10;++block){big.clear();a.processBlock(big,p);for(int i=0;i<256;++i){single.clear();b.processBlock(single,p);}}
 for(int block=0;block<10;++block){for(int c=0;c<2;++c)for(int i=0;i<256;++i)big.setSample(c,i,.8f);a.processBlock(big,p);for(int i=0;i<256;++i){single.clear();b.processBlock(single,p);check(big.getSample(0,i)==single.getSample(0,0));}}
 // Pure dry unity, non-finite target ignored, empty and mono blocks.
 Parameters dry;dry.dryWet=0;GranularProcessor d;d.prepare(48000,4,dry);
 single.setSample(0,0,.125f);single.setSample(1,0,-.25f);d.processBlock(single,dry);check(single.getSample(0,0)==.125f&&single.getSample(1,0)==-.25f);
 dry.pitchSemitones=std::numeric_limits<float>::quiet_NaN();juce::AudioBuffer<float> mono(1,19);mono.clear();d.processBlock(mono,dry);juce::AudioBuffer<float> empty(2,0);d.processBlock(empty,dry);
 std::cout<<"PASS: envelopes; deterministic/finite stress at 5 rates; allocation guard; wrap; partition invariance; wet energy; freeze isolation; dry unity; mono/empty; invalid target\n";
}catch(const std::exception& e){allocationForbidden=false;std::cerr<<e.what()<<'\n';return 1;}}
