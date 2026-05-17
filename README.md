# PulseForge

PulseForge is a source-available, non-commercial Qt Widgets PipeWire audio
enhancement app for Linux. It creates a PulseForge virtual sink, captures that
monitor stream, processes audio in its own C++ DSP engine, and plays the result
to the selected real PipeWire output.

## License

PulseForge is licensed under the
[PolyForm Noncommercial License 1.0.0](LICENSE).

This means you can read, share, modify, and redistribute the code for permitted
non-commercial purposes, but commercial packaging, resale, paid redistribution,
or other commercial use is not allowed without separate permission from the
copyright holder.

Redistributions must keep the license and required notices, including
attribution to the official repository:

```text
https://github.com/Moe-Ara/PulseForge
```

Because the license restricts commercial use, PulseForge is source-available but
not OSI-open-source software.

## Runtime Dependencies

- Qt 6 Widgets runtime
- PipeWire, PipeWire Pulse, and WirePlumber
- PulseAudio-compatible PipeWire tools (`pactl`)
- `systemd --user` for optional start-on-login support

On Fedora/Bazzite, the runtime packages are typically:

```bash
sudo dnf install qt6-qtbase pipewire pipewire-pulseaudio wireplumber
```

Build dependencies:

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel pipewire-devel pkgconf-pkg-config
```

PulseForge does not use PipeWire filter-chain, LADSPA, LV2, EasyEffects, or
external DSP plugins.

## Architecture

The current routing path is:

```text
Apps
-> PulseForge virtual sink
-> pulseforge_enhanced.monitor
-> PulseForge AudioProcessor
-> selected real output sink
```

The code is organized around focused modules:

- `src/audio/`: backend lifecycle, PipeWire device discovery, and routing.
- `src/dsp/`: real-time audio processor, SPSC frame buffer, EQ, limiter, and compressor.
- `src/system/`: process execution, runtime state, and autostart integration.
- `src/core/`: application-wide configuration and logging.
- `src/gui/`: Qt Widgets UI and reusable components.

For deeper implementation notes, real-time audio rules, and contribution
workflow, see [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

## Install

For a user-local install on Fedora/Bazzite:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

This installs:

- `~/.local/bin/pulseforge`
- `~/.local/share/applications/pulseforge.desktop`
- `~/.local/share/icons/hicolor/scalable/apps/pulseforge.svg`
- `~/.local/share/metainfo/io.github.Moe_Ara.PulseForge.metainfo.xml`
- `~/.local/share/pulseforge/styleDark.qss`
- `~/.local/share/systemd/user/pulseforge.service`

Refresh desktop/app metadata after a user-local install:

```bash
update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
```

For distro packaging, use the normal prefix:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
DESTDIR="$PWD/package-root" cmake --install build
```

The CMake install rules place files under standard Fedora paths:

- `/usr/bin/pulseforge`
- `/usr/share/applications/pulseforge.desktop`
- `/usr/share/icons/hicolor/scalable/apps/pulseforge.svg`
- `/usr/share/metainfo/io.github.Moe_Ara.PulseForge.metainfo.xml`
- `/usr/share/pulseforge/styleDark.qss`
- `/usr/share/systemd/user/pulseforge.service`

Suggested RPM runtime requirements:

```text
qt6-qtbase
pipewire
pipewire-pulseaudio
wireplumber
systemd
```

Suggested RPM build requirements:

```text
cmake
gcc-c++
qt6-qtbase-devel
pipewire-devel
pkgconfig
```

Packagers should run these validation tools when available:

```bash
desktop-file-validate data/pulseforge.desktop
appstreamcli validate --no-net data/metainfo/io.github.Moe_Ara.PulseForge.metainfo.xml
```

## Start On Login

PulseForge uses `systemd --user`; no `sudo` is required.

```bash
systemctl --user daemon-reload
systemctl --user enable --now pulseforge.service
```

To disable automatic startup:

```bash
systemctl --user disable pulseforge.service
```

The service launches `pulseforge --background`, which currently starts the app minimized and is ready for future tray/background mode.

## Usage

1. Launch PulseForge from the desktop menu or run `pulseforge`.
2. Select the real output device you want to hear.
3. Pick a preset or adjust the equalizer.
4. Press the main enhancement button.

PulseForge stores settings with Qt settings and keeps crash-cleanup state in
`$XDG_RUNTIME_DIR/pulseforge/runtime-state`.

## For Developers

PulseForge is built around a small number of important boundaries:

- `AudioService` is the app-facing orchestration layer.
- `PipeWireBackend` owns PipeWire lifecycle, virtual sink routing, and cleanup.
- `AudioProcessor` owns real-time stream capture/playback and DSP processing.
- `PresetFactory` maps presets and GUI controls into an `EffectChain`.
- GUI components publish user intent; they should not duplicate DSP logic.

Important development rules:

- Keep audio callbacks allocation-free, lock-free, and log-free.
- Implement DSP inside PulseForge rather than through external plugin chains.
- Preserve enable/disable cleanup and previous-default-sink restoration.
- Use reusable Qt Widgets components and object-name based QSS.
- Add new constants to `AppConfig`, `AudioConfig`, or `DspConfig` where possible.

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) before working on the audio
backend, DSP engine, preset system, or GUI architecture.

## Contributing

Contributions are welcome. Good first areas include:

- Testing PulseForge on different PipeWire/WirePlumber desktop sessions.
- Improving presets for common headphones, speakers, and handheld devices.
- Polishing Qt Widgets layout and accessibility.
- Adding focused DSP improvements with real-time-safe implementations.
- Improving Fedora/Bazzite packaging metadata and validation.

Before submitting a change:

1. Build the project with CMake.
2. Test enable/disable cycles with real playback.
3. Confirm the previous default sink is restored after disable.
4. Keep changes focused and explain user-visible behavior in the pull request.

Please avoid adding external DSP plugin dependencies. PulseForge’s goal is to
own its audio processing path in clean, maintainable C++.

## Contributors

PulseForge is maintained as a source-available, non-commercial Linux audio
enhancement project.

Project creator and maintainer:

- Moe Ara

Add yourself here in pull requests that make a meaningful contribution.
