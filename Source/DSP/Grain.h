#pragma once
#include <cstdint>
namespace granular
{
struct Grain
{
    double readPosition = 0.0; // Unwrapped sample position.
    int age = 0;
    int length = 0;           // Immutable until this voice finishes.
    float pitchRandom = 0.0f; // Fixed random seeds in [-1, 1].
    float panRandom = 0.0f;
    float harmonic = 0.0f;
    float direction = 1.0f;
    std::uint64_t visualId=0,birthSample=0;
    std::uint32_t colour=0;
    int mood=2;
    float sourceEnergy=0;
    bool isActive() const noexcept { return age < length; }
};
}
