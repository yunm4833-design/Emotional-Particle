#pragma once
#include <juce_graphics/juce_graphics.h>
#include "FontData.h"
namespace emotional {
inline juce::Font font(float size,float tracking=0) {
 static auto face=juce::Typeface::createSystemTypefaceFor(fontData,sizeof(fontData));
 juce::Font f(face);f.setHeight(size);f.setExtraKerningFactor(tracking);return f;
}
}
