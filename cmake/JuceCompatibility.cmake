# Two compatibility fixes already present in the 0.4 dependency, now reproducible.
# No DSP or GUI implementation is patched; retain the upstream pinned revision.
set(header "${JUCE_SOURCE_DIR}/modules/juce_audio_basics/juce_audio_basics.h")
file(READ "${header}" text)
if(NOT text MATCHES "#include <type_traits>")
 string(REPLACE "#pragma once" "#pragma once\n#include <type_traits>" text "${text}")
 file(WRITE "${header}" "${text}")
endif()
set(header "${JUCE_SOURCE_DIR}/modules/juce_audio_processors/processors/juce_AudioPluginInstance.h")
file(READ "${header}" text)
string(REGEX REPLACE "    template <size_t numLayouts>\n    AudioPluginInstance \\(const short channelLayoutList\\[numLayouts\\]\\[2\\]\\) : AudioProcessor \\(channelLayoutList\\) \\{\\}\n" "" patched "${text}")
if(NOT patched STREQUAL text)
 file(WRITE "${header}" "${patched}")
endif()
