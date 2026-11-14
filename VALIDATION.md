# 0.5 validation record

Local Linux/GCC 14 native CMake build: **6/6 CTest suites passed**, 14.41 seconds total. JUCE and project source were compiled together (no cached 0.4 module library). Historical read-bound diagnostics were enabled for this local run.

- Plugin: mono/stereo layouts; 44.1/48/96 kHz; empty and variable blocks; state and parameters; unity parameter bypass; nonfinite audio recovery.
- Mood: all mood-pair 800 ms transitions at five sample rates; proportional Intensity; harmonics/reverse; rapid changes and frozen read history.
- Visual: 500,000 concurrent FIFO transfers; bounded overflow; audio equality with/without telemetry; stale End handling; editor-closed recovery; realtime allocation guard.
- Detail: detail mapping and Intensity saturation behavior; neutral equivalence; actual sound changes under detail controls.
- Core: window endpoints; deterministic stress; ring wrap; partition invariance; freeze isolation; dry unity.
- Stability: synchronous host echo; concurrent automated parameter changes with audio; 80 editor lifecycles; 320 actual UI paints; nonfinite parameter recovery. An additional run processed 94,299 concurrent audio blocks without test failure (the count varies with machine scheduling).

Actual JUCE screenshots rendered successfully, including the expanded detail panel, live UI Pitch binding and Reset Details. The preview had six visible real grains; last measured visualizer software paint was 8.052 ms. This is one Linux measurement, not a frame-rate guarantee.

The host-echo fixture was also linked against 0.4: it failed the bounded test with repeated notifications. Passing the same fixture after the change verifies the regression fix but does not reproduce the exact Live crash.

Windows/MSVC pluginval, macOS universal/AU/auval, and full ASan/UBSan runs are configured in `.github/workflows/build.yml`; their actual outcomes are recorded in GitHub Actions. Do not treat a configured workflow as a passed check. No AAX SDK, AAX binary, PACE signature or Pro Tools validation is available in this environment. Live 12.4.16 on Windows still needs a real retest.

See `docs/STABILITY.md` for the limits of the supplied crash-dump analysis. The original crash report and personal data are excluded from Git.
