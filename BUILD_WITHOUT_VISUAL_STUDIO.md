# Build Simple DSP on GitHub (no local Visual Studio needed)

The package contains source files and the GitHub Actions workflow, **not** a finished Windows executable.

1. Extract `SimpleDSP_v0_3_GitHub_Update.zip`.
2. In your existing `creativedamages/SimpleDSP` repository, replace existing files with **everything inside the extracted folder**, preserving this structure:

   ```text
   .github/workflows/build-windows.yml
   Source/DSP.h
   Source/Main.cpp
   Source/RemotePage.h
   Source/RemoteServer.h
   tests/dsp_test.cpp
   assets/SimpleDSP.ico
   assets/SimpleDSP-Original.png
   assets/apple-touch-icon.png
   assets/favicon-32.png
   assets/icon-192.png
   assets/icon-256.png
   assets/icon-512.png
   CMakeLists.txt
   README.md
   BUILD_WITHOUT_VISUAL_STUDIO.md
   build-windows.bat
   build-windows.ps1
   ```

3. Commit the replacements to the default branch (`main`). On macOS, Finder can show the hidden `.github` folder using **Command+Shift+.**.
4. Go to **Actions → Build Windows application**. Either use the new run started by the commit or **Run workflow**.
5. When the run is green, open it and download the artifact called **SimpleDSP-Windows-x64**. Extract and run `Simple DSP.exe` on Windows 10/11 x64.
6. Keep the EXE in a stable location before enabling **Start with Windows**.

If the GitHub build is red, open the earliest failing step and examine its compiler errors. The Windows app has not yet been compiled or hardware-tested here. Existing `.dsp.json` presets and the startup configuration are intentionally preserved in `%APPDATA%\RobbyApps\DualChannelDSP\`.
