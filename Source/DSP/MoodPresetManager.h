#pragma once
#include "GranularProcessor.h"
namespace granular {
struct MoodPreset {
 const char* name;
 const char* subtitle;
 GranularProcessor::Frame values;
 std::uint32_t background, accent, secondary;
};
struct DetailParameters {
 float sizeScale=1,densityScale=1,scatterScale=1,reverseBias=0;
 float pitchShift=0,positionBias=0,jitterScale=1,widthScale=1;
 float toneScale=1,mixBias=0,motionScale=1,driveDb=0;
};
class MoodPresetManager final {
public:
 using DSP=GranularProcessor;
 static const std::array<MoodPreset,8> presets;
 static DSP::Frame targetFor(int mood,float intensity,bool freeze,const DetailParameters& detail={}) noexcept;
 void prepare(double sr,int mood,float intensity,bool freeze,const DetailParameters& detail={}) noexcept;
 void setTarget(int mood,float intensity,bool freeze,const DetailParameters& detail={}) noexcept;
 DSP::Frame next() noexcept;
private:
 DSP::Frame lower {}, upper {};
 std::array<juce::SmoothedValue<float,juce::ValueSmoothingTypes::Linear>,DSP::Count> smooth;
};
}
