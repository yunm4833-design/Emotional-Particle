JUCE 7.0.12: https://github.com/juce-framework/JUCE
Pinned commit: 4f43011b96eb0636104cb3e433894cda98243626
Fetched by CMake. Complete source can be obtained at that commit. The source is available in build/_deps/juce-src after a default build.
Reproducible local compatibility changes in cmake/JuceCompatibility.cmake: explicitly include <type_traits> in juce_audio_basics.h; remove an unused legacy AudioPluginInstance C-array constructor rejected by newer Clang. Replace the legacy native-window screenshot helper with Cocoa NSView caching because CGWindowListCreateImage is unavailable in macOS SDK 15+. This does not alter DSP or component painting.
JUCE may be used under GPLv3 or its applicable commercial license; see JUCE's LICENSE.md and module headers. GPL-3.0.txt is retained in this repository.
VST3 SDK is bundled inside JUCE under its retained license; see modules/juce_audio_processors/format_types/VST3_SDK.

Embedded Questrial-Regular.ttf: Google Fonts / Questrial, SIL Open Font License 1.1. Source and license in Assets; runtime data in Source/UI/FontData.h.
https://github.com/google/fonts/tree/main/ofl/questrial

Avid AAX SDK and PACE tools are NOT included or downloaded by this project. Optional AAX integration requires separately licensed SDK/tools. It has not been signed or validated in retail Pro Tools.
