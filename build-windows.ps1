$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
Write-Host 'Built: build/EmotionalParticle_artefacts/Release/VST3/Emotional Particle.vst3'
