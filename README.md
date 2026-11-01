# MediaTable

A floating KDE Plasma media player with an interactive vinyl record, album artwork mode, and desktop-aware visibility.

**Current baseline:** 0.5.37  
**Target platform:** KDE Plasma 6 on Wayland (tested by the author on Bazzite)

## Features

- Interactive vinyl record with playback interaction.
- Choose between **Vinyl** (default), **Album** artwork, or **Nothing**.
- MPRIS media-player integration.
- Desktop card remains visible while its screen area is clear and gets out of the way when another window covers it.
- Independent top-left hot corner and media-event popup behaviour.
- Right-click settings menu with appearance, behaviour, and desktop options.
- Dark, light, or system-matched theme; adjustable opacity and corner radius.
- Adjustable hover expansion, animation speed, and temporary popup timeout.
- Preferences persist across restarts and updates.

## Requirements

- KDE Plasma 6 running on Wayland.
- Fedora-based host with `toolbox` (Bazzite is the primary tested target).
- `kpackagetool6`, `kwriteconfig6`, and `qdbus6` or `qdbus` on the host.
- Internet access during the first install so Toolbox can install build dependencies.

The installer creates/uses a Fedora Toolbox named `kde-media-flyout-build` and installs CMake, GCC, Qt 6 development packages, and LayerShellQt **inside the Toolbox**, not on the immutable host.

## Install

1. Download and extract the source archive.
2. Open a terminal in the extracted `mediatable` directory.
3. Run:

   ```bash
   ./install.sh
   ```

4. Start MediaTable:

   ```bash
   ./run.sh
   ```

The first installation builds the C++ overlay in Toolbox and installs/enables the KWin popup-area monitoring script.

**Source archive:** [MediaTable 0.5.37 source package](dist/mediatable-0.5.37.tar.gz).

## Test mode

To show a test card without an active media player:

```bash
./run.sh --test
```

## Update

Run `./update.sh` from the extracted project directory. It rebuilds the application and updates the KWin helper. Keep the project directory in a stable location if your Autostart entry points to its `run.sh`.

## Autostart

In **System Settings → Session → Autostart**, add the top-level `run.sh` as a login script. The overlay becomes useful once an MPRIS player publishes media information.

## Settings

Right-click the visible player to open MediaTable settings.

- **Artwork:** Vinyl (default), Album, or Nothing.
- **Theme:** Match system, Dark, or Light.
- **Background opacity:** 85%, 92%, 96%, or 100%.
- **Corner radius:** 12, 17, 22, or 26 px.
- **Hover expansion:** on/off, with Instant, Fast, Normal, or Slow animation.
- **Temporary popup auto-hide:** 2, 4, 8 seconds, or Never.
- **Desktop visibility:** optionally hide the desktop card when foreground windows cover its footprint.
- **Reset to defaults:** restores the original Vinyl/Dark setup.

Settings are stored through Qt `QSettings` under `THXS/MediaTable` and survive restarts and updates.

## Uninstall

Run:

```bash
./uninstall.sh
```

This removes the MediaTable build and KWin helper but leaves the source directory and Toolbox container intact. To remove the build container too:

```bash
toolbox rm --force kde-media-flyout-build
```

The optional `remove-old-widget.sh` removes the legacy KDE Media Flyout Plasma widget package. Remove old widget instances from the desktop/panels first; the standalone MediaTable does not require that widget.

## Project layout

```text
mediatable/
├── install.sh
├── update.sh
├── run.sh
├── uninstall.sh
├── remove-old-widget.sh
└── overlay/kde-media-flyout-overlay/
    ├── CMakeLists.txt
    ├── src/main.cpp
    ├── thxs-media-flyout-visibility/
    ├── install.sh
    ├── run.sh
    └── uninstall.sh
```

## License

The overlay component includes its MIT licence at `overlay/kde-media-flyout-overlay/LICENSE`. No repository-wide licence has been selected yet.
