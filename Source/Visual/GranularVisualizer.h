#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "VisualTelemetry.h"
namespace granular {
// Message-thread owned. Voice slot + unique ID prevents stale End events killing new grains.
class GrainVisualModel final {
public:
 struct Particle {GrainEvent metadata;bool active=false,placed=false;float x=0,y=0,vx=0,vy=0;};
 std::array<Particle,128> particles {};
 std::uint64_t clock=0;
 std::uint32_t generation=0;
 void ingest(VisualTelemetry&) noexcept;
 float phase(const Particle&) const noexcept;
 float envelope(const Particle&) const noexcept;
 int activeCount() const noexcept;
 void clear() noexcept {for(auto& p:particles)p.active=p.placed=false;}
};
class GranularVisualizer final:public juce::Component,private juce::Timer {
public:
 explicit GranularVisualizer(VisualTelemetry&);
 ~GranularVisualizer() override;
 void setTheme(juce::Colour background,juce::Colour accent,juce::Colour secondary,int mood);
 void setEffectEnabled(bool enabled) noexcept {effectEnabled=enabled;}
 void refreshNow(); // Message thread only; also used by offline UI verification.
 int visibleGrainCount() const noexcept {return visible;}
 double lastPaintMilliseconds() const noexcept {return lastPaintMs;}
 void paint(juce::Graphics&) override;
private:
 void timerCallback() override {refreshNow();}
 VisualTelemetry& telemetry;
 GrainVisualModel model;
 struct FlowMote {float u,y,seed,radius,depth;};
 std::array<FlowMote,4800> flow {};
 std::array<ScopePoint,256> history {};
 unsigned historyWrite=0;
 std::array<juce::SmoothedValue<float>,8> motion;
 juce::Colour background{0xff201723},accent{0xffedc591},secondary{0xffd49cdb};
 int mood=2,visible=0;
 bool effectEnabled=true;
 float rms=0,time=0;
 double lastFrame=0,lastClockChange=0,lastPaintMs=0;
 std::uint64_t lastClock=0;
};
}
