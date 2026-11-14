#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DSP/MoodPresetManager.h"
class EmotionalProcessor final : public juce::AudioProcessor {
public:
 EmotionalProcessor();
 void prepareToPlay(double,int) override;
 void releaseResources() override {}
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 juce::AudioProcessorEditor* createEditor() override;
 juce::AudioProcessorParameter* getBypassParameter() const override {return state.getParameter("bypass");}
 bool hasEditor() const override{return true;}
 const juce::String getName() const override{return "Emotional Particle";}
 bool acceptsMidi() const override{return false;}
 bool producesMidi() const override{return false;}
 double getTailLengthSeconds() const override{return std::numeric_limits<double>::infinity();}
 int getNumPrograms() override{return 8;}
 int getCurrentProgram() override;
 void setCurrentProgram(int) override;
 const juce::String getProgramName(int) override;
 void changeProgramName(int,const juce::String&) override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;
 juce::AudioProcessorValueTreeState state;
 granular::VisualTelemetry visualTelemetry;
 std::atomic<float> meter{0}, activity{0};
 std::atomic<int> voices{0};
private:
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
 float parameter(int) const noexcept;
 void process(juce::AudioBuffer<float>&,juce::MidiBuffer&,bool);
 granular::DetailParameters details() const noexcept;
 granular::GranularProcessor engine;
 granular::MoodPresetManager moods;
 juce::SmoothedValue<float> outputGain,bypassBlend;
 std::array<std::atomic<float>*,17> raw{};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EmotionalProcessor)
};
