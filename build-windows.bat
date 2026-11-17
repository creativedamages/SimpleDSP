@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: CMake is not installed or not on PATH.
  exit /b 1
)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
if errorlevel 1 exit /b 1
cmake --build build --config Release --parallel 4
if errorlevel 1 exit /b 1
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 exit /b 1
echo.
echo Built executables:
dir /s /b build\HT DSP.exe
echo.
pause
