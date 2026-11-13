# Build and download the Windows app without installing development software

The project includes a **GitHub Actions build workflow**. GitHub runs the Microsoft compiler on a Windows build machine. **Visual Studio is not required on your PC.**

1. Sign in to [GitHub](https://github.com/) (free account works for a public repository) and create a new repository, e.g. `dual-channel-dsp`. **Do not initialize it with a README**, so the upload page remains simple.
2. Extract this ZIP. In the repository, choose **uploading an existing file** and upload the extracted files and folders, **including `.github/workflows/build-windows.yml`**. Commit the upload to the default branch. If GitHub's web uploader omits the hidden `.github` directory, create `.github/workflows/build-windows.yml` using GitHub's **Add file > Create new file** menu and paste the workflow content from the ZIP.
3. Click **Actions** > **Build Windows application** > **Run workflow** (if it wasn't automatically run by the upload). The Windows runner downloads JUCE, compiles the application, and runs tests.
4. Open the completed Actions run. Under **Artifacts**, download `Dual-Channel-DSP-Windows-x64` and extract it. Double-click **Dual Channel DSP.exe**.

**Important:** The Windows GUI build has not been run in this environment, so the first CI build may reveal a Windows/JUCE compilation issue. No Windows binary is included in the supplied ZIP. A green build verifies compilation and DSP unit tests, **not** correct physical I/O mapping or PA safety with your specific audio interface.

For ASIO, first install the manufacturer's Windows driver. Select ASIO in the app; assign one physical input and one *different* output to each mono processing channel. There is no summing and no output limiter; test with amplifiers muted and levels down.
