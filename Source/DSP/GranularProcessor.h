#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>
#include "Grain.h"
#include "../Visual/VisualTelemetry.h"

namespace granular
{
struct Parameters
{
    float grainMs = 80.0f;        // 1 .. 500
    float densityHz = 20.0f;      // 1 .. 100
    float pitchSemitones = 0.0f;  // -24 .. 24
    float position = 0.0f;        // 0: recent safe history; 1: oldest safe history
    float timeSprayMs = 30.0f;    // 0 .. 500
    float pitchJitter = 0.0f;     // 0 .. 12 semitones
    float panJitter = 0.5f;       // 0 .. 1
    float dryWet = 0.5f;          // 0 .. 1, linear mix
    float windowMorph = 0.0f;     // 0: Hann; 1: Gaussian; 2: Trapezoid
    bool freeze = false;
};

// All methods belong to the audio/lifecycle thread. GUI communicates via APVTS.
class GranularProcessor final
{
public:
    static constexpr int maxGrains = 128;
    void attachVisualTelemetry(VisualTelemetry* sink) noexcept {telemetry=sink;}
    void setVisualContext(std::uint32_t colour,int mood) noexcept {visualColour=colour;visualMood=mood;}
    std::uint64_t getSampleClock() const noexcept {return sampleClock;}
    void prepare(double sampleRate, double bufferSeconds = 4.0,
                 const Parameters& initial = {});
    void reset() noexcept; // Lifecycle/transport reset only; clears captured audio.
    void processBlock(juce::AudioBuffer<float>&, const Parameters&) noexcept;
    int getActiveGrainCount() const noexcept;
    static float envelope(float phase, float morph) noexcept;

public:
    enum Index { Size, Rate, Pitch, Position, TimeSpray, PitchJitter,
                 PanJitter, Mix, Window, Freeze, Spread, Reverse, Rhythm, LowOct, Fifth, HighOct, WowDepth, WowHz, FlutterDepth, FlutterHz, Tone, Edge, DriveDb, Count };
    using Frame = std::array<float, Count>;
    std::array<float, 2> processSmoothedSample(float left, float right, const Frame&) noexcept;
    static Frame fromParameters(const Parameters&) noexcept;
    static float windowEnvelope(float,float,float) noexcept;
    static Frame clampFrame(const Frame&) noexcept;
private:
    using Smoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    void setTargets(const Parameters&) noexcept;
    Frame nextFrame() noexcept;
    void spawn(const Frame&) noexcept;
    void emit(GrainEventKind,const Grain&,const Frame&,float pan,float pitch,float phase,float env) noexcept;
    void captureScope(float,float) noexcept;
    float readCubic(int channel, double position) const noexcept;
    float randomBipolar() noexcept;

    VisualTelemetry* telemetry=nullptr;
    std::uint64_t sampleClock=0,nextVisualId=1;
    std::uint32_t generation=0,visualColour=0xffedc591;
    int visualMood=2,visualInterval=400,scopeInterval=32,scopeCount=0;
    float levelCoefficient=.01f,scopeSum=0,scopeLow=0,scopeHigh=0;
    double scopeEnergy=0;
    juce::AudioBuffer<float> ring;
    std::array<Grain, maxGrains> grains {};
    std::array<Smoother, Count> smooth;
    std::array<float, 2> recorded {};
    double sampleRate = 48000.0;
    double triggerPhase = 0.0;
    double triggerThreshold = 1.0;
    double wowPhase = 0.0, flutterPhase = 0.0;
    std::array<float,2> toneState {};
    std::int64_t writePosition = 0; // Next write; advances only while capturing.
    int capacity = 0;
    int validSamples = 0;
    std::uint32_t randomState = 0x73ac2e19u;
};
}
