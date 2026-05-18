# PulseForge

PulseForge is a source-available, non-commercial Linux desktop audio enhancer
for PipeWire. It creates a virtual audio sink, captures that monitor stream,
processes the audio in PulseForge's own C++ DSP engine, and plays the result
through your selected real output device.

Current release target: `v0.1.0-alpha`

## Alpha Status

PulseForge is early alpha software. The core routing and DSP path are intended
to be usable, but you should expect rough edges across different PipeWire,
WirePlumber, device, and desktop-session setups.

Before reporting audio bugs, test that disable/quit restores your previous
default output device and that no stale `pulseforge_enhanced` sink remains.

## Features

- Qt 6 Widgets desktop UI with dark theme and tray support.
- PipeWire output device selection.
- PulseForge virtual sink with C++ DSP processing.
- Live EQ, presets, preamp, limiter, compressor, and stereo enhancement controls.
- Mic isolation guard so the PulseForge monitor does not become the default mic.
- Persistent settings for device, preset, EQ, enhancement state, and startup behavior.
- User-level systemd start-on-login integration.
- AppImage-oriented packaging structure.

PulseForge does not use PipeWire filter-chain, LADSPA, LV2, EasyEffects, or
external DSP plugins.

## License

PulseForge is licensed under the
[PolyForm Noncommercial License 1.0.0](LICENSE).

You can read, share, modify, and redistribute the code for permitted
non-commercial purposes. Commercial packaging, resale, paid redistribution, or
other commercial use is not allowed without separate permission from the
copyright holder.

Redistributions must keep the license and required notices, including
attribution to the official repository:

```text
https://github.com/Moe-Ara/PulseForge
```

Because the license restricts commercial use, PulseForge is source-available but
not OSI-open-source software.

## Runtime Requirements

Required at runtime:

- PipeWire
- WirePlumber
- PipeWire Pulse compatibility server
- `pactl`
- Qt 6 Widgets runtime

Optional:

- `systemd --user` for Start on login

On Fedora/Bazzite, these are usually present. If needed:

```bash
sudo dnf install qt6-qtbase pipewire pipewire-pulseaudio wireplumber pulseaudio-utils
```

PulseForge checks these requirements on startup and shows a friendly error if
the audio session is not ready.

## Install AppImage

Download the AppImage from the GitHub release page, then:

```bash
chmod +x PulseForge-v0.1.0-alpha-x86_64.AppImage
./PulseForge-v0.1.0-alpha-x86_64.AppImage
```

The AppImage bundles PulseForge and Qt libraries. It does not bundle PipeWire,
WirePlumber, `pipewire-pulse`, or system audio services.

## Build AppImage

Install build tools, Qt development headers, PipeWire development headers,
`linuxdeploy`, and `linuxdeploy-plugin-qt`, then run:

```bash
scripts/package-appimage.sh
```

The script builds:

- `PulseForge-v0.1.0-alpha-x86_64.AppImage`
- `PulseForge-v0.1.0-alpha-x86_64.AppImage.sha256`

## Build From Source

Fedora/Bazzite build dependencies:

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel pipewire-devel pkgconf-pkg-config
```

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/pulseforge
```

User-local install:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel
cmake --install build
```

System/package install layout:

```text
/usr/bin/pulseforge
/usr/share/applications/io.github.MoeAra.PulseForge.desktop
/usr/share/icons/hicolor/256x256/apps/pulseforge.png
/usr/share/icons/hicolor/scalable/apps/pulseforge.svg
/usr/share/metainfo/io.github.MoeAra.PulseForge.metainfo.xml
/usr/share/pulseforge/styleDark.qss
/usr/share/systemd/user/pulseforge.service
```

Validate packaging metadata when the tools are installed:

```bash
desktop-file-validate data/io.github.MoeAra.PulseForge.desktop
appstreamcli validate --no-net data/metainfo/io.github.MoeAra.PulseForge.metainfo.xml
```

## Start On Login

Use the `Start on login` checkbox in the PulseForge UI.

Internally, PulseForge uses a user-level systemd service and does not require
`sudo`:

```bash
systemctl --user enable pulseforge.service
systemctl --user disable pulseforge.service
systemctl --user is-enabled pulseforge.service
```

For AppImage use, PulseForge writes a user service that points at the currently
running AppImage path and launches with `--start-minimized`.

## Usage

1. Launch PulseForge from the app menu, AppImage, or terminal.
2. Select the real output device you want to hear.
3. Pick a preset or adjust EQ/effect controls.
4. Press the large power button to enable enhancement.
5. Close the window to keep PulseForge in the tray, or use Quit to fully exit.

Command-line startup options:

```bash
pulseforge --start-minimized
pulseforge --background
pulseforge --minimized
```

## Architecture

Current routing:

```text
Apps
-> PulseForge virtual sink
-> pulseforge_enhanced.monitor
-> PulseForge AudioProcessor
-> selected real output sink
```

Code layout:

- `src/audio/`: PipeWire backend lifecycle, device discovery, and routing.
- `src/dsp/`: real-time audio processor, SPSC frame buffer, EQ, limiter, and compressor.
- `src/system/`: process execution, settings, runtime state, dependency checks, and autostart.
- `src/core/`: app constants and logging.
- `src/gui/`: Qt Widgets UI, tray manager, and reusable components.
- `data/`: desktop file, icon, AppStream metadata, and systemd service template.
- `scripts/`: release/build helper scripts.

Developer notes are in [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). DSP notes
are in [docs/DSP_AUDIT.md](docs/DSP_AUDIT.md).

## Troubleshooting

### PulseForge Says Runtime Requirements Are Missing

Make sure PipeWire, WirePlumber, PipeWire Pulse, and `pactl` are installed and
running:

```bash
systemctl --user status pipewire.service pipewire-pulse.service wireplumber.service
pactl info
```

On Fedora/Bazzite:

```bash
sudo dnf install pipewire pipewire-pulseaudio wireplumber pulseaudio-utils
```

Then log out and back in.

### No Audio After Enabling

Use the tray menu or the main button to disable enhancement. PulseForge should
restore your previous default sink. If not, manually select your real output in
your desktop sound settings and restart PulseForge.

### Chat Apps Hear PulseForge Output

Do not select `pulseforge_enhanced.monitor` as your microphone in Discord or
other chat apps. Select your real microphone. PulseForge also guards against
that monitor becoming the system default source.

### Stale Virtual Sink After Crash

PulseForge stores crash-cleanup state in:

```text
$XDG_RUNTIME_DIR/pulseforge/runtime-state
```

On next launch it tries to unload stale modules and restore defaults.

## Release Process

See [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md).

Release assets for GitHub:

- AppImage
- SHA256 checksum
- Source tarball
- Screenshots
- Release notes

Generate a checksum manually:

```bash
sha256sum PulseForge-v0.1.0-alpha-x86_64.AppImage > PulseForge-v0.1.0-alpha-x86_64.AppImage.sha256
```

## Contributing

Contributions are welcome, especially:

- Fedora/Bazzite testing.
- PipeWire/WirePlumber compatibility fixes.
- DSP tuning that stays real-time safe.
- UI polish and accessibility.
- Packaging metadata and AppImage validation.

Before submitting a change:

1. Build the project with CMake.
2. Test enable/disable cycles with real playback.
3. Confirm the previous default sink is restored after disable and quit.
4. Confirm no stale PulseForge virtual sinks remain after restart.
5. Keep changes focused and explain user-visible behavior.

Please avoid external DSP plugin dependencies. PulseForge’s goal is to own its
audio processing path in clean, maintainable C++.

## Roadmap

- Better alpha QA across Fedora, Bazzite, KDE, GNOME, and common USB devices.
- Flatpak and RPM packaging after the AppImage path is stable.
- More robust output stream recovery after device hotplug.
- Improved loudness/gain staging and safer high-intensity presets.
- Accessibility pass for keyboard navigation and screen readers.
- Optional background/tray-first mode polish.

## Contributors

Project creator and maintainer:

- Moe Ara

Add yourself here in pull requests that make a meaningful contribution.

buymeacoffee.com/moeara