#include "DSP/MoodPresetManager.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <new>
#include <stdexcept>
static bool forbid=false;
void* operator new(std::size_t n){if(forbid)std::abort();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
#define CHECK(x) do {if(!(x))throw std::runtime_error("Failed at line "+std::to_string(__LINE__));}while(false)
int main(){try{
 using D=granular::GranularProcessor;using M=granular::MoodPresetManager;
 for(int mood=0;mood<8;++mood){auto full=M::targetFor(mood,1,false),half=M::targetFor(mood,.5f,false),zero=M::targetFor(mood,0,false);
  for(int index:{D::Mix,D::TimeSpray,D::PitchJitter,D::PanJitter,D::Spread,D::Rhythm,D::WowDepth,D::FlutterDepth}){CHECK(half[index]==full[index]*.5f);CHECK(zero[index]==0);}
  CHECK(full[D::Size]>=1&&full[D::Size]<=500);
 }
 CHECK(M::targetFor(4,1,false)[D::Reverse]==.5f);
 CHECK(M::targetFor(0,1,false)[D::LowOct]==.14f);
 CHECK(M::targetFor(2,1,false)[D::HighOct]==.45f);
 for(double sr:{8000.,44100.,48000.,96000.,192000.}){
  for(int from=0;from<8;++from)for(int to=0;to<8;++to){
   M m;m.prepare(sr,from,.35f,false);m.setTarget(to,1,false);auto start=M::targetFor(from,.35f,false);auto goal=M::targetFor(to,1,false);D::Frame f;
   int n=int(std::floor(sr*.8));for(int s=0;s<n;++s){f=m.next();for(int k=0;k<D::Count;++k)CHECK(f[k]>=std::min(start[k],goal[k])-.01f&&f[k]<=std::max(start[k],goal[k])+.01f);}
   CHECK(f==goal);
  }
  D engine;engine.prepare(sr);M m;m.prepare(sr,0,1,false);double energy=0;
  for(int s=0;s<int(sr*12);++s){
   if(s%int(sr/2)==0)m.setTarget((s/int(sr/2))%8,(s/int(sr/2))%3*.5f,false);
   float x=.4f*std::sin(s*.027f)+.1f*std::sin(s*.17f);forbid=true;auto v=engine.processSmoothedSample(x,-x,m.next());forbid=false;
   CHECK(std::isfinite(v[0])&&std::isfinite(v[1]));energy+=double(v[0])*v[0];
  }
  CHECK(energy>1);
  // Worst-case length/speed; reverse in both directions, moving writer, freeze/unfreeze.
  auto f=M::targetFor(4,1,false);f[D::Size]=500;f[D::Spread]=0;f[D::Pitch]=24;f[D::Rate]=100;
  for(int s=0;s<int(sr*8);++s){f[D::Freeze]=(s/int(sr))%2?1.f:0.f;auto v=engine.processSmoothedSample(.2f,-.2f,f);CHECK(std::isfinite(v[0]));}
 }
 std::cout<<"PASS: exact 800ms all-pair mood transitions at five rates, intensity proportionality, harmony/reverse matrix, rapid switching, allocation guard, reverse read bounds and freeze\n";
}catch(const std::exception& e){forbid=false;std::cerr<<e.what()<<'\n';return 1;}}
