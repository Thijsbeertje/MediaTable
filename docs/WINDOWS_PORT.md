# MediaTable for Windows 11

The Windows port is being developed on the `windows-port` branch. The Linux implementation remains the reference and must continue to work as before.

## Current status

The Windows build reuses the existing MediaTable Qt player UI, artwork rendering, vinyl interaction, animations, and settings. It uses a frameless, translucent, always-on-top Qt window and a separate top-left summon target.

Windows media discovery and playback controls are implemented through the Windows Global System Media Transport Controls (GSMTC) API. The backend reads session metadata, album artwork, playback state, timeline position, and available controls, and supports play/pause, previous/next, seeking, and switching between sessions. WinRT calls run on a dedicated worker thread so the Qt interface remains responsive.

The `--test` preview remains available and shows a persistent sample card for up to 60 seconds. Run the app normally (without `--test`) to discover Windows media sessions.

## Download and run

Download the **MediaTable-Windows-preview** artifact from the latest successful Windows build workflow on the `windows-port` branch. Extract the ZIP once.

For real media playback, start Spotify (or another app with Windows media-session support) and double-click `Run-MediaTable.bat`. MediaTable will show the active session when Windows exposes it. The launcher keeps a command window open and writes startup output to `MediaTable-startup.log`.

For a visual-only demonstration, double-click `Run-MediaTable-Preview.bat`. This displays a sample card and does not connect to media sessions. If the normal launcher finds no player, share the log contents so the issue can be diagnosed.

Keep the Qt DLLs and plugin folders next to `MediaTable.exe`; do not move the executable out of the extracted folder.

## Build requirements (only if building from source)

- Windows 10/11 x64
- Visual Studio 2022 with **Desktop development with C++**
- Qt 6.5 or newer, **MSVC 2022 64-bit**
- Qt modules: **Widgets** and **Network**
- Windows SDK with C++/WinRT headers and the Windows Runtime import libraries
- CMake 3.20 or newer

From the repository root in PowerShell:

```powershell
cmake -S overlay/kde-media-flyout-overlay -B build -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release
```

## Next milestones

1. Test GSMTC discovery, artwork, playback controls, seeking, and multiple sessions on Windows 11.
2. Recreate desktop-aware visibility and test hot-corner behaviour across displays and DPI settings.
3. Add polished packaging, Qt runtime deployment, and autostart support.

## Guardrails

- Do not modify the default Linux branch as part of this port.
- Preserve the existing Linux executable name and Toolbox launch path.
- Keep platform-specific code isolated.
- Do not describe Windows support as complete until it has been built and tested on Windows 11.
