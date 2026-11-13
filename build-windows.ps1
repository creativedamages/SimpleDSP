$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build build --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
ctest --test-dir build -C Release --output-on-failure
Write-Host 'Windows application:'
Get-ChildItem -Path build -Filter 'Simple DSP.exe' -Recurse | Select-Object -ExpandProperty FullName
