#pragma once
#include <juce_graphics/juce_graphics.h>
#include <cmath>

namespace emotional {
// Filled annular sector: avoids PathStrokeHelpers in the frequently repainted knobs.
inline void fillRing(juce::Graphics& g, juce::Point<float> centre, float radius,
                     float thickness, float start, float end) {
    if (!std::isfinite(end) || end <= start || radius <= thickness * .5f) return;
    constexpr int segments = 96;
    juce::Path path;
    path.preallocateSpace((segments + 1) * 6 + 16);
    const float outer = radius + thickness * .5f, inner = radius - thickness * .5f;
    for (int n = 0; n <= segments; ++n) {
        const float a = start + (end - start) * float(n) / segments;
        const auto p = centre + juce::Point<float>(std::sin(a), -std::cos(a)) * outer;
        if (n == 0) path.startNewSubPath(p); else path.lineTo(p);
    }
    for (int n = segments; n >= 0; --n) {
        const float a = start + (end - start) * float(n) / segments;
        path.lineTo(centre + juce::Point<float>(std::sin(a), -std::cos(a)) * inner);
    }
    path.closeSubPath();
    g.fillPath(path);
}
}
