# AAX integration

AAX is an optional JUCE target, not an included SDK or a validated release.

1. Obtain the Avid AAX SDK under the appropriate Avid developer agreement, and install the SDK on a Windows/macOS build machine. Do not commit it to GitHub.
2. Configure with `-DEP_ENABLE_AAX=ON -DAAX_SDK_PATH=/absolute/path/to/AAX_SDK`. Configuration fails explicitly when `Interfaces/AAX.h` is missing. For macOS universal builds also pass `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`; the SDK and all required SDK libraries must support both architectures.
3. Build the `EmotionalParticle_AAX` target with the platform's native toolchain.
4. Use the applicable Avid/PACE tools and credentials to sign/wrap the AAX bundle. Test in a compatible Pro Tools environment; development-host exceptions do not establish that retail Pro Tools will load an unsigned bundle.
5. Only publish an AAX installer/bundle after that validation. No AAX SDK, signing credentials, PACE tools or unsigned AAX artifact is placed in the public workflow.

The manual AAX workflow requires a maintainer-managed self-hosted runner labelled `aax` with `AAX_SDK_PATH` configured locally. It is not run on untrusted pull requests. Signing remains on the licensed machine.
