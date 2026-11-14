#include "MoodPresetManager.h"
#include <algorithm>
#include <cmath>
namespace granular {
const std::array<MoodPreset,8> MoodPresetManager::presets {{
 // size, rate, pitch, position, timeSpray, pitchJitter, pan, mix, window, freeze,
 // sizeSpread, reverse, rhythm, lowOct, fifth, highOct, wowDepth, wowHz, flutterDepth, flutterHz, tone, edge, driveDb
 {"Melancholic","RAIN AFTER MIDNIGHT",{160,48,0,.28f,120,.08f,.45f,.78f,1,0, 40,0,.08f,.14f,.10f,0,.03f,.16f,0,5,2600,.18f,0},0xff151724,0xff8ba9e8,0xff6d608a},
 {"Anxious","FRAGMENTS IN MOTION",{15,85,0,.08f,450,12,1,.70f,2,0, 10,.12f,.95f,0,.05f,.03f,0,.5f,0,7,14000,.04f,0},0xff140b10,0xffff3458,0xff9d234b},
 {"Dreamy","FLOATING MEMORIES",{80,65,0,.32f,200,.12f,1,.85f,.35f,0, 20,0,.12f,0,.10f,.45f,.08f,.12f,.01f,5,10000,.25f,0},0xff071613,0xffa8eac8,0xffe2c68d},
 {"Calm","ROOM TO BREATHE",{420,5,0,.35f,30,.04f,.30f,.65f,0,0, 80,0,.05f,0,0,0,.12f,.12f,.01f,3,14000,.45f,0},0xff17221d,0xffa6bda7,0xffeee6d3},
 {"Nostalgic","TRACES ON TAPE",{85,22,0,.20f,80,.15f,.50f,.72f,.6f,0, 25,.50f,.22f,.05f,0,0,.25f,.33f,.035f,5.8f,4200,.20f,0},0xff261b15,0xffdca45f,0xff946749},
 {"Hopeful","A LIGHT AHEAD",{60,40,0,.18f,95,.10f,.8f,.75f,0,0, 15,0,.08f,0,.25f,.25f,.06f,.18f,.01f,4,12000,.2f,0},0xff0d1e20,0xffb8efd4,0xffefd49b},
 {"Lonely","ECHOES WITHOUT ANSWERS",{280,7,0,.55f,340,.05f,.7f,.78f,1,0, 60,.22f,.3f,.28f,0,0,.09f,.08f,.008f,3,1800,.3f,0},0xff121a29,0xff8daacc,0xff596eac},
 {"Furious","PRESSURE INTO FIRE",{18,95,-5,.1f,230,3,.8f,.85f,2,0, 10,.35f,.65f,.18f,.06f,0,.12f,.4f,.04f,8,6500,.06f,9},0xff260e0b,0xffff784a,0xfffbc373}
}};
MoodPresetManager::DSP::Frame MoodPresetManager::targetFor(int mood,float intensity,bool freeze,const DetailParameters& detail) noexcept {
 auto f=presets[std::clamp(mood,0,7)].values;
 f[DSP::Size]*=detail.sizeScale;f[DSP::Spread]*=detail.sizeScale;
 f[DSP::Rate]*=detail.densityScale;f[DSP::TimeSpray]*=detail.scatterScale;
 f[DSP::Reverse]+=detail.reverseBias;f[DSP::Pitch]+=detail.pitchShift;
 f[DSP::Position]+=detail.positionBias;f[DSP::PitchJitter]*=detail.jitterScale;
 f[DSP::PanJitter]*=detail.widthScale;f[DSP::Tone]*=detail.toneScale;
 f[DSP::Mix]=std::clamp(f[DSP::Mix]+detail.mixBias,0.f,1.f);
 for(int n:{DSP::Rhythm,DSP::WowDepth,DSP::FlutterDepth})f[n]*=detail.motionScale;
 f[DSP::DriveDb]+=detail.driveDb;
 f=DSP::clampFrame(f);
 const float i=std::isfinite(intensity)?std::clamp(intensity,0.f,1.f):0.f;
 for(int index:{DSP::Mix,DSP::TimeSpray,DSP::PitchJitter,DSP::PanJitter,DSP::Spread,DSP::Rhythm,DSP::WowDepth,DSP::FlutterDepth})f[index]*=i;
 f[DSP::Freeze]=freeze?1.f:0.f;
 return DSP::clampFrame(f);
}
void MoodPresetManager::prepare(double sr,int mood,float intensity,bool freeze,const DetailParameters& detail) noexcept {
 const auto f=targetFor(mood,intensity,freeze,detail);
 lower=upper=f;
 for(int n=0;n<DSP::Count;++n){smooth[n].reset(sr,.8);smooth[n].setCurrentAndTargetValue(f[n]);}
}
void MoodPresetManager::setTarget(int mood,float intensity,bool freeze,const DetailParameters& detail) noexcept {
 const auto f=targetFor(mood,intensity,freeze,detail);
 for(int n=0;n<DSP::Count;++n)if(smooth[n].getTargetValue()!=f[n]){
  const float current=std::clamp(smooth[n].getCurrentValue(),lower[n],upper[n]);
  lower[n]=std::min(current,f[n]);upper[n]=std::max(current,f[n]);
  smooth[n].setCurrentAndTargetValue(current);smooth[n].setTargetValue(f[n]);
 }
}
MoodPresetManager::DSP::Frame MoodPresetManager::next() noexcept {
 DSP::Frame f;for(int n=0;n<DSP::Count;++n)f[n]=std::clamp(smooth[n].getNextValue(),lower[n],upper[n]);return f;
}
}
