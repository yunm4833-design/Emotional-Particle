#include "DSP/MoodPresetManager.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
void wave(const std::string& path,const std::vector<float>& v){std::ofstream out(path,std::ios::binary);auto u16=[&](unsigned x){out.put(char(x));out.put(char(x>>8));};auto u32=[&](unsigned x){u16(x);u16(x>>16);};out.write("RIFF",4);u32(36+unsigned(v.size()*2));out.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);out.write("data",4);u32(unsigned(v.size()*2));for(float x:v)u16(unsigned(short(std::clamp(x,-1.f,1.f)*32767)));}
int main(int argc,char** argv){const auto directory=argc>1?std::filesystem::path(argv[1]):std::filesystem::current_path()/"Audio";using D=granular::GranularProcessor;using M=granular::MoodPresetManager;std::filesystem::create_directories(directory);
 for(int mood=-1;mood<8;++mood){D e;e.prepare(48000);M manager;manager.prepare(48000,std::max(0,mood),1,false);std::vector<float> v;v.reserve(48000*8*2);float peak=0;double energy=0;
  for(int s=0;s<48000*12;++s){double t=s/48000.;float env=float(.2+.8*std::exp(-6*std::fmod(t,.5)));double p=juce::MathConstants<double>::twoPi*55*t;float x=env*(.25f*std::sin(p+1.6*std::sin(p*2))+.08f*std::sin(p*5.01));auto wet=e.processSmoothedSample(x,x,manager.next());if(s>=48000*4){for(int c=0;c<2;++c){float y=mood<0?x*.501187f:wet[c]*.501187f;peak=std::max(peak,std::abs(y));energy+=double(y)*y;v.push_back(y);}}}
  std::string name=mood<0?"00-Dry":std::to_string(mood+1)+std::string("-")+M::presets[mood].name;wave((directory/(name+".wav")).string(),v);std::cout<<name<<" peak="<<peak<<" rms="<<std::sqrt(energy/v.size())<<'\n';if(peak>1)return 1;
 }
}
