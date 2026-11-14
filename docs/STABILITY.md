# Stability investigation — 0.5

## Evidence from the user-provided Live report

The newest dump in the supplied archive belongs to Live 12.4.16, 2026-11-14. Older unrelated dumps in the same archive were not used to attribute this crash.

- Exception code: `0xc0000005` (access violation).
- Exception instruction pointer: `ntdll.dll + 0x3f6cf`.
- The faulting thread's saved stack contains addresses in the 0.4 plug-in's drawing path. Symbol lookup against the retained 0.4 Windows binary maps these RVAs to:

| Plug-in RVA | Nearest symbol |
| --- | --- |
| `0xe47a2` | `juce::PathStrokeHelpers::createStroke` |
| `0xe8397` | `juce::Graphics::strokePath` |
| `0x45a4b3` | `EmotionalLook::drawRotarySlider` |
| `0x267908` | `juce::Slider::Pimpl::paint` |
| `0x1a103f`, `0x1a12ca` | `juce::Component::paintComponentAndChildren` |
| `0x279243` | `juce::HWNDComponentPeer::performPaint` |

The captured stack also contains Windows heap and CRT addresses. This is a **raw stack-address scan with nearest-symbol lookup**, not a fully symbolized/unwound Windows debugger backtrace. Some entries may be stale. It supports investigating the GUI allocation/paint path, but does not prove which instruction first corrupted memory, or prove that the audio thread is fault-free. The allocation failure is not a demonstrated program-notification recursion; those are separate findings.

The original archive contains personal project/configuration information and is deliberately not committed. No personal paths, serials, projects, or raw dumps are published.

## Changes and regression checks

1. Same-program `setCurrentProgram` calls are idempotent. A synthetic host listener that synchronously echoes the current program triggered 32 nested notifications in the bounded pre-fix test; the fixed implementation emits one. This reproduces a reentrancy risk, not the exact reported Live crash.
2. Program indices and DSP-facing parameter reads are finite/range checked before rounding, array lookup or gain conversion. Invalid sample-rate setup has a safe fallback. Empty/malformed state blobs are rejected.
3. The host bypass callback uses the same smoothed dry path as the bypass parameter and keeps DSP capture running.
4. Rotary progress arcs use filled sectors rather than `Graphics::strokePath`. This removes the rotary arc stroking path implicated by the stack scan without claiming that all Windows heap failures are fixed.
5. Windows CI uses the official MSVC toolchain, not the experimental MinGW cross build used for 0.4. Matching PDB files are archived for future reports.
6. Tests cover a synchronous host echo, concurrent parameter automation during audio processing, 80 editor creation/destruction cycles and 320 actual UI paints. Existing FIFO, window, freeze, read-history, state, parameter mapping and morph tests remain enabled.
7. Linux sanitizer CI instruments JUCE and project sources with AddressSanitizer and UndefinedBehaviorSanitizer. It does not establish freedom from data races (not ThreadSanitizer) or certify Windows/macOS drivers.

## Live retest procedure

Close Live before replacing the full VST3 bundle. Keep a backup of the original project. Test first with a blank set, one instance and a known audio clip. Run playback while rapidly changing all eight moods, moving Intensity/Grain/Density, toggling Freeze/Bypass and opening/closing the editor. Repeat with several instances and the original set. Record the exact 0.5 Git commit/build, Live version, sample rate/block size and the action at failure.

If it crashes, retain the new Live report and the matching `Validation-Windows` artifact (PDB files). Report whether the same clip and automation crash with the UI closed. Do not infer that 'tests pass' means the original host failure is solved; that requires a real Live retest.
