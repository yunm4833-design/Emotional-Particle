#include "GranularProcessor.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstdlib>

namespace granular
{
void GranularProcessor::prepare(double sr, double seconds, const Parameters& p)
{
    if (!std::isfinite(sr) || sr < 8000.0 || sr > 384000.0)
        throw std::invalid_argument("Unsupported sample rate");
    sampleRate = sr;
    visualInterval=std::max(1,int(sr/120));scopeInterval=std::max(1,int(sr/1500));
    levelCoefficient=float(1-std::exp(-1/(sr*.003)));
    seconds = std::isfinite(seconds) ? std::clamp(seconds, 4.0, 30.0) : 4.0;
    capacity = static_cast<int>(std::ceil(sr * seconds));
    ring.setSize(2, capacity); // Allocation is allowed ONLY in prepare.
    for (auto& s : smooth) s.reset(sr, 0.03);
    setTargets(p);
    reset();
}

void GranularProcessor::reset() noexcept
{
    ++generation;scopeCount=0;scopeSum=scopeLow=scopeHigh=0;scopeEnergy=0;
    if(telemetry){GrainEvent e;e.kind=GrainEventKind::Reset;e.generation=generation;e.sampleStamp=sampleClock;telemetry->send(e);telemetry->audioClock.store(sampleClock,std::memory_order_release);}
    ring.clear();
    grains = {};
    recorded = {};
    writePosition = validSamples = 0;
    triggerPhase = 0.0;
    triggerThreshold = 1.0;
    wowPhase = flutterPhase = 0.0;
    toneState = {};
    randomState = 0x73ac2e19u;
    for (auto& s : smooth) s.setCurrentAndTargetValue(s.getTargetValue());
}

GranularProcessor::Frame GranularProcessor::fromParameters(const Parameters& p) noexcept
{
    return {p.grainMs,p.densityHz,p.pitchSemitones,p.position,p.timeSprayMs,p.pitchJitter,
            p.panJitter,p.dryWet,p.windowMorph,p.freeze?1.f:0.f,
            0,0,0,0,0,0,0,.2f,0,5,18000,.15f,0};
}
GranularProcessor::Frame GranularProcessor::clampFrame(const Frame& f) noexcept
{
    constexpr Frame lo {1,1,-24,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,.01f,0,.1f,80,.02f,0};
    constexpr Frame hi {500,100,24,1,500,12,1,1,2,1, 100,1,1,1,1,1,2,2,1,20,20000,.5f,24};
    Frame result {};
    for(int i=0;i<Count;++i) result[i]=std::isfinite(f[i])?std::clamp(f[i],lo[i],hi[i]):lo[i];
    return result;
}
void GranularProcessor::setTargets(const Parameters& p) noexcept
{
    const auto f=clampFrame(fromParameters(p));
    for(int i=0;i<Count;++i) smooth[i].setTargetValue(f[i]);
}

GranularProcessor::Frame GranularProcessor::nextFrame() noexcept
{
    Frame f {};
    for (int i = 0; i < Count; ++i) f[i] = smooth[i].getNextValue();
    return f;
}

float GranularProcessor::randomBipolar() noexcept
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return 2.0f * static_cast<float>(randomState >> 8) / 16777216.0f - 1.0f;
}

void GranularProcessor::spawn(const Frame& f) noexcept
{
    const float duration = std::clamp(f[Size] + randomBipolar()*f[Spread], 1.f, 500.f);
    const int n = std::max(2, static_cast<int>(std::lround(duration * sampleRate * .001)));
    const bool reverse = (randomBipolar()+1)*.5f < f[Reverse];
    const double travel = 4.0*(n-1);
    // Entire lifetime is safe even if pitch ramps to +24 st and capture freezes.
    const double minimumDelay = reverse ? 4.0 : travel+4.0;
    // Also reserve n future writes so the writer cannot overwrite this voice.
    const double maximumDelay = std::min(validSamples - 2, capacity - n - 4) - (reverse?travel:0.0);
    if (maximumDelay < minimumDelay) return; // Warm-up: insufficient history.

    for (auto& g : grains)
    {
        if (g.isActive()) continue;
        const double nominal = minimumDelay + f[Position] * (maximumDelay - minimumDelay);
        const double jitter = randomBipolar() * f[TimeSpray] * sampleRate * .001;
        const double delay = std::clamp(nominal + jitter, minimumDelay, maximumDelay);
        g.readPosition = static_cast<double>(writePosition) - delay;
        g.age = 0;g.visualId=nextVisualId++;g.birthSample=sampleClock;
        g.colour=visualColour;g.mood=visualMood;g.sourceEnergy=0;
        g.length = n;
        g.pitchRandom = randomBipolar();
        g.panRandom = randomBipolar();
        g.direction = reverse ? -1.f : 1.f;
        const float choice = (randomBipolar()+1)*.5f;
        g.harmonic = choice<f[LowOct] ? -12.f : choice<f[LowOct]+f[Fifth] ? 7.f
                   : choice<f[LowOct]+f[Fifth]+f[HighOct] ? 12.f : 0.f;
        return;
    }
    // Pool full: skip trigger. Never steal a voice that is still audible.
}

float GranularProcessor::readCubic(int channel, double position) const noexcept
{
    const auto i = static_cast<std::int64_t>(std::floor(position));
    jassert(i - 1 >= writePosition - validSamples && i + 2 < writePosition);
#if defined(GRANULAR_VALIDATE_READS)
    if (!(i - 1 >= writePosition - validSamples && i + 2 < writePosition)) std::abort();
#endif
    const float t = static_cast<float>(position - static_cast<double>(i));
    const auto* data = ring.getReadPointer(channel);
    const auto at = [&](std::int64_t index) noexcept
    {
        index %= capacity;
        if (index < 0) index += capacity;
        return data[static_cast<int>(index)];
    };
    const float a = at(i - 1), b = at(i), c = at(i + 1), d = at(i + 2);
    return b + .5f * t * (c - a + t * (2*a - 5*b + 4*c - d
                                        + t * (3*(b - c) + d - a)));
}

float GranularProcessor::envelope(float x,float morph) noexcept {return windowEnvelope(x,morph,.15f);}
float GranularProcessor::windowEnvelope(float x, float morph, float edgeFraction) noexcept
{
    if (x <= 0.0f || x >= 1.0f) return 0.0f;
    const float hann = .5f - .5f * std::cos(juce::MathConstants<float>::twoPi * x);
    constexpr float sigma = .18f;
    const float floor = std::exp(-.5f * .25f / (sigma * sigma));
    const float z = (x - .5f) / sigma;
    const float gaussian = (std::exp(-.5f * z*z) - floor) / (1.0f - floor);
    const float edge = std::clamp(std::min(x, 1.0f - x) / edgeFraction, 0.0f, 1.0f);
    const float trapezoid = edge * edge * (3.0f - 2.0f * edge);
    // Continuous convex morph; each constituent is zero at BOTH endpoints.
    morph = std::clamp(morph, 0.0f, 2.0f);
    return morph <= 1.0f ? hann + morph * (gaussian - hann)
                        : gaussian + (morph - 1.0f) * (trapezoid - gaussian);
}

void GranularProcessor::processBlock(juce::AudioBuffer<float>& audio,
                                     const Parameters& parameters) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = audio.getNumChannels();
    if (capacity == 0 || channels < 1 || channels > 2) return;
    setTargets(parameters);
    auto* left = audio.getWritePointer(0);
    auto* right = channels == 2 ? audio.getWritePointer(1) : nullptr;

    for (int s=0; s<audio.getNumSamples(); ++s)
    {
        const auto v=processSmoothedSample(left[s],right?right[s]:left[s],nextFrame());
        left[s]=right?v[0]:.5f*(v[0]+v[1]);
        if(right)right[s]=v[1];
    }
}

std::array<float,2> GranularProcessor::processSmoothedSample(float inL,float inR,const Frame& f) noexcept
{
    if(capacity==0)return {inL,inR};
    if(!std::isfinite(inL))inL=0;
    if(!std::isfinite(inR))inR=0;
    if(telemetry)captureScope(inL,inR);
        if (f[Freeze] < 1.0f)
        {
            // Smooth capture into a hold, and hold into capture on thaw.
            // Once Freeze reaches 1, BOTH writes and writePosition stop.
            const float capture = 1.0f - f[Freeze];
            recorded[0] += capture * (inL - recorded[0]);
            recorded[1] += capture * (inR - recorded[1]);
            const int index = static_cast<int>(writePosition % capacity);
            ring.setSample(0, index, recorded[0]);
            ring.setSample(1, index, recorded[1]);
            ++writePosition;
            validSamples = std::min(validSamples + 1, capacity);
        }

        triggerPhase += f[Rate] / sampleRate;
        if (triggerPhase >= triggerThreshold)
        {
            triggerPhase -= triggerThreshold;
            triggerThreshold = 1.0 + .9*randomBipolar()*f[Rhythm];
            spawn(f);
        }

        wowPhase = std::fmod(wowPhase + f[WowHz]/sampleRate, 1.0);
        flutterPhase = std::fmod(flutterPhase + f[FlutterHz]/sampleRate, 1.0);
        const float drift = f[WowDepth]*std::sin(juce::MathConstants<double>::twoPi*wowPhase)
                          + f[FlutterDepth]*std::sin(juce::MathConstants<double>::twoPi*flutterPhase);
        float wetL = 0.0f, wetR = 0.0f;
        for (auto& g : grains)
        {
            if (!g.isActive()) continue;
            const float x = static_cast<float>(g.age) / static_cast<float>(g.length - 1);
            const float env = windowEnvelope(x, f[Window], f[Edge]);
            const float pan = g.panRandom * f[PanJitter];
            const float sourceL=readCubic(0,g.readPosition),sourceR=readCubic(1,g.readPosition);
            wetL += sourceL * env * std::sqrt(1.0f - pan);
            wetR += sourceR * env * std::sqrt(1.0f + pan);
            const float pitch = std::clamp(f[Pitch] + g.harmonic + drift + g.pitchRandom * f[PitchJitter], -24.0f, 24.0f);
            if(telemetry){
                g.sourceEnergy+=levelCoefficient*(.5f*(sourceL*sourceL+sourceR*sourceR)-g.sourceEnergy);
                const int stride=std::max(1,std::min(visualInterval,g.length/8));
                if(g.age==0 || g.age%stride==0 || g.age==g.length-1)
                    emit(g.age==0?GrainEventKind::Birth:GrainEventKind::Update,g,f,pan,pitch,x,env);
                if(g.age==g.length-1)emit(GrainEventKind::End,g,f,pan,pitch,x,0);
            }
            g.readPosition += g.direction * std::exp2(pitch / 12.0f);
            ++g.age;
        }

        // Smooth expected overlap compensation; no discontinuous active-count divisor.
        const float overlap = f[Rate] * f[Size] * .001f;
        const float gain = 1.0f / std::sqrt(std::max(1.0f, overlap));
        wetL *= gain;
        wetR *= gain;
        if(f[DriveDb]>0){
            const float drive=std::pow(10.f,f[DriveDb]/20.f),blend=f[DriveDb]/24.f;
            wetL+=blend*(std::tanh(wetL*drive)/std::sqrt(drive)-wetL);
            wetR+=blend*(std::tanh(wetR*drive)/std::sqrt(drive)-wetR);
        }
        const float a = 1.f-std::exp(-juce::MathConstants<float>::twoPi*std::min(f[Tone],float(sampleRate*.42))/float(sampleRate));
        toneState[0] += a*(wetL-toneState[0]);toneState[1] += a*(wetR-toneState[1]);
        ++sampleClock;
        if(telemetry && sampleClock%std::uint64_t(visualInterval)==0)telemetry->audioClock.store(sampleClock,std::memory_order_release);
        return {(1.f-f[Mix])*inL+f[Mix]*toneState[0], (1.f-f[Mix])*inR+f[Mix]*toneState[1]};
}

void GranularProcessor::emit(GrainEventKind kind,const Grain& g,const Frame& f,float pan,float pitch,float phase,float env) noexcept
{
    GrainEvent e;e.kind=kind;e.id=g.visualId;e.birthSample=g.birthSample;e.sampleStamp=sampleClock;
    e.generation=generation;e.colour=g.colour;e.slot=std::uint16_t(&g-grains.data());
    e.pan=pan;e.pitch=pitch;e.duration=float(g.length/sampleRate);e.length=g.length;
    e.envelopePhase=phase;e.envelopeGain=env;e.sampleRate=sampleRate;e.mood=g.mood;
    e.readOffset=std::clamp(float((writePosition-g.readPosition)/capacity),0.f,1.f);
    e.sourceLevel=std::sqrt(std::max(0.f,g.sourceEnergy));e.wet=f[Mix];e.windowMorph=f[Window];e.windowEdge=f[Edge];
    telemetry->send(e);
}
void GranularProcessor::captureScope(float l,float r) noexcept
{
    const float mono=.5f*(l+r);scopeSum+=mono;
    scopeLow=std::min(scopeLow,mono);scopeHigh=std::max(scopeHigh,mono);
    scopeEnergy+=.5*(double(l)*l+double(r)*r);
    if(++scopeCount==scopeInterval){
        telemetry->send(ScopePoint{scopeSum/scopeCount,scopeLow,scopeHigh,float(std::sqrt(scopeEnergy/scopeCount))});
        scopeCount=0;scopeSum=scopeLow=scopeHigh=0;scopeEnergy=0;
    }
}

int GranularProcessor::getActiveGrainCount() const noexcept
{
    int count = 0;
    for (const auto& g : grains) count += g.isActive() ? 1 : 0;
    return count;
}
}
