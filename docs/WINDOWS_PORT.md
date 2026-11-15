# MediaTable for Windows 11

The Windows port is being developed on the `windows-port` branch. The Linux implementation remains the reference and must continue to work as before.

## Current status

The Windows preview build reuses the existing MediaTable Qt player UI, artwork rendering, vinyl interaction, animations, settings, and test card. It uses a frameless, translucent, always-on-top Qt window and a separate top-left summon target. Windows media-session discovery and desktop occlusion detection are not implemented yet.

The preview shows a persistent expanded sample card for up to 60 seconds, rather than auto-hiding like a normal temporary popup.

## Download and run

Download the **MediaTable-Windows-preview** artifact from the latest successful Windows build workflow on the `windows-port` branch. Extract the ZIP once, then double-click `Run-MediaTable-Preview.bat`.

The preview should appear near the top-left of the primary display. The launcher keeps a command window open and writes startup output to `MediaTable-startup.log`. If no player window appears, share the log contents so the startup failure can be diagnosed.

Keep the Qt DLLs and plugin folders next to `MediaTable.exe`; do not move the executable out of the extracted folder.

## Build requirements (only if building from source)

- Windows 10/11 x64
- Visual Studio 2022 with **Desktop development with C++**
- Qt 6.5 or newer, **MSVC 2022 64-bit**
- Qt modules: **Widgets** and **Network**
- CMake 3.20 or newer

From the repository root in PowerShell:

```powershell
cmake -S overlay/kde-media-flyout-overlay -B build -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release
```

## Next milestones

1. Implement a Windows Global System Media Transport Controls (GSMTC) provider for metadata and playback.
2. Recreate desktop-aware visibility and test hot-corner behaviour across displays and DPI settings.
3. Add packaging, Qt runtime deployment, and autostart support.

## Guardrails

- Do not modify the default Linux branch as part of this port.
- Preserve the existing Linux executable name and Toolbox launch path.
- Keep platform-specific code isolated.
- Do not describe Windows support as complete until it has been built and tested on Windows 11.
