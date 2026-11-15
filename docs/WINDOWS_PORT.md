# MediaTable for Windows 11

The Windows port is being developed on the `windows-port` branch. The Linux implementation remains the reference and must continue to work as before.

## Current status

The first Windows milestone is a Qt-based preview build:

- Reuses the existing MediaTable player UI, artwork rendering, vinyl interaction, animations, settings, and test card.
- Uses a frameless, translucent, always-on-top Qt window on Windows.
- Keeps a separate top-left summon target.
- Builds without Linux-only D-Bus and LayerShellQt dependencies.
- Does not yet include Windows media-session discovery or desktop occlusion detection.

The preview launcher displays the existing sample card for 60 seconds, then exits automatically.

## Download and run

Download the **MediaTable-Windows-preview** artifact from the latest successful Windows build workflow on the `windows-port` branch. Extract the downloaded ZIP once, then double-click:

```text
Run-MediaTable-Preview.bat
```

The batch file starts `MediaTable.exe --test`. Keep the Qt DLLs and plugin folders next to the executable; do not move the executable out of the extracted folder.

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

1. **Windows media sessions:** implement a Windows Global System Media Transport Controls (GSMTC) provider and map session metadata, artwork, playback state, position, and transport controls into the shared player model.
2. **Desktop-aware behaviour:** replace the KWin visibility helper with Windows foreground-window/occlusion monitoring; verify the hot corner and always-on-top behaviour.
3. **Packaging:** add Windows autostart support and a Windows installer.

## Guardrails

- Do not modify the default Linux branch as part of this port.
- Preserve the existing Linux executable name and Toolbox launch path.
- Keep platform-specific code isolated.
- Do not describe Windows support as complete until it has been built and tested on Windows 11.
