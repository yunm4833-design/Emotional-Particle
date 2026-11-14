#include "DSP/MoodPresetManager.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#define CHECK(x) do{if(!(x))throw std::runtime_error("Detail failure at line "+std::to_string(__LINE__));}while(false)
int main(){try{using D=granular::GranularProcessor;using M=granular::MoodPresetManager;granular::DetailParameters n;
 for(int m=0;m<8;++m){auto f=M::targetFor(m,1,false,n);CHECK(f==D::clampFrame(M::presets[m].values));}
 granular::DetailParameters d;d.sizeScale=1.3f;d.densityScale=.6f;d.scatterScale=.4f;d.reverseBias=.4f;d.pitchShift=6;d.positionBias=.1f;d.jitterScale=3;d.widthScale=.5f;d.toneScale=1.7f;d.mixBias=-.2f;d.motionScale=.4f;d.driveDb=7;
 auto f=M::targetFor(0,1,false,d);CHECK(std::abs(f[D::Size]-208)<.01f);CHECK(std::abs(f[D::Rate]-28.8f)<.01f);CHECK(f[D::Pitch]==6);CHECK(std::abs(f[D::TimeSpray]-48)<.01f);CHECK(f[D::Reverse]==.4f);CHECK(std::abs(f[D::Tone]-4420)<.1f);CHECK(f[D::DriveDb]==7);
 d.sizeScale=2;d.densityScale=2;d.scatterScale=2;d.reverseBias=.5f;d.pitchShift=24;d.positionBias=.5f;d.jitterScale=4;d.widthScale=1.5f;d.toneScale=2;d.mixBias=.5f;d.motionScale=2;d.driveDb=18;
 for(int m=0;m<8;++m){auto full=M::targetFor(m,1,false,d),half=M::targetFor(m,.5f,false,d),zero=M::targetFor(m,0,false,d);for(int index:{D::Mix,D::TimeSpray,D::PitchJitter,D::PanJitter,D::Spread,D::Rhythm,D::WowDepth,D::FlutterDepth}){CHECK(half[index]==full[index]*.5f);CHECK(zero[index]==0);}}
 D a,b;a.prepare(48000);b.prepare(48000);M ma,mb;ma.prepare(48000,7,1,false,d);mb.prepare(48000,7,1,false);double difference=0;
 for(int s=0;s<48000*6;++s){float x=.3f*std::sin(s*.03f);auto va=a.processSmoothedSample(x,x,ma.next()),vb=b.processSmoothedSample(x,x,mb.next());CHECK(std::isfinite(va[0]));difference+=std::abs(va[0]-vb[0]);}CHECK(difference>100);
 std::cout<<"PASS: 12 detail mappings, neutral preset equivalence, saturated-range intensity scaling, real audio difference and finite extreme DSP\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
