#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <memory>
#include <cstdint>
#include <type_traits>
namespace granular {
// Exactly one audio producer and one message-thread consumer. Never reset live.
template<typename T,int Capacity> class SpscFifo final {
 static_assert(std::is_trivially_copyable<T>::value);
 juce::AbstractFifo fifo{Capacity+1};
 std::unique_ptr<std::array<T,Capacity+1>> data=std::make_unique<std::array<T,Capacity+1>>();
public:
 bool push(const T& value) noexcept {int a,b,c,d;fifo.prepareToWrite(1,a,b,c,d);if(b==0)return false;(*data)[a]=value;fifo.finishedWrite(1);return true;}
 bool pop(T& value) noexcept {int a,b,c,d;fifo.prepareToRead(1,a,b,c,d);if(b==0)return false;value=(*data)[a];fifo.finishedRead(1);return true;}
};
enum class GrainEventKind : std::uint8_t { Birth, Update, End, Reset };
struct GrainEvent {
 std::uint64_t id=0,birthSample=0,sampleStamp=0;
 std::uint32_t generation=0,colour=0;
 std::uint16_t slot=0;
 GrainEventKind kind=GrainEventKind::Update;
 float pan=0,pitch=0,duration=0,envelopePhase=0,envelopeGain=0;
 float readOffset=0,sourceLevel=0,wet=0,windowMorph=0,windowEdge=.15f;
 double sampleRate=48000;
 int length=0,mood=2;
};
struct ScopePoint {float mean=0,minimum=0,maximum=0,rms=0;};
class VisualTelemetry final {
 static_assert(std::atomic<std::uint64_t>::is_always_lock_free,"Requires lock-free 64-bit atomics");
 static_assert(std::atomic<std::uint32_t>::is_always_lock_free,"Requires lock-free counters");
public:
 SpscFifo<GrainEvent,4096> grains;
 SpscFifo<ScopePoint,2048> scope;
 std::atomic<std::uint64_t> audioClock{0};
 std::atomic<std::uint32_t> droppedGrains{0},droppedScope{0};
 void send(const GrainEvent& e) noexcept {if(!grains.push(e))droppedGrains.fetch_add(1,std::memory_order_relaxed);}
 void send(const ScopePoint& p) noexcept {if(!scope.push(p))droppedScope.fetch_add(1,std::memory_order_relaxed);}
};
}
