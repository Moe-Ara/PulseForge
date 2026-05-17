# PulseForge Development Guide

This guide is for contributors working on PulseForge internals. PulseForge is a
source-available, non-commercial Qt 6 Widgets desktop app with a PipeWire-backed
audio pipeline and an in-process C++ DSP engine.

## Project Goals

PulseForge should feel like a lightweight Linux-native audio enhancer:

- Qt Widgets only, no QML.
- PipeWire-first on Linux desktop sessions.
- No PipeWire filter-chain, LADSPA, LV2, EasyEffects, or external DSP plugins.
- Real-time audio processing owned by PulseForge.
- Clean, modular code with small focused classes.
- Modern dark UI using reusable widgets and QSS.

## License Expectations

PulseForge uses the PolyForm Noncommercial License 1.0.0. Contributions are made
with the understanding that the project remains source-available for
non-commercial use unless the maintainer explicitly changes licensing later.

Do not remove or weaken the required notices in `NOTICE`. Redistributed copies
must credit the official repository:

```text
https://github.com/Moe-Ara/PulseForge
```

## Repository Layout

```text
src/
  audio/      PipeWire backend lifecycle, device discovery, routing, audio config
  core/       application constants and logging
  dsp/        AudioProcessor, EQ, dynamics, ring buffer, DSP config
  gui/        MainWindow and Qt Widgets components
  presets/    built-in presets and user preset persistence
  service/    app-facing AudioService orchestration
  system/     process runner, runtime state, autostart support
data/         desktop file, icon, AppStream metadata, systemd user service
docs/         contributor and architecture notes
```

## Build

Fedora/Bazzite build dependencies:

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel pipewire-devel pkgconf-pkg-config
```

Configure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Run from the build tree:

```bash
./build/pulseforge
```

Install locally:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

## Audio Architecture

Current runtime path:

```text
Apps
-> PulseForge virtual sink
-> pulseforge_enhanced.monitor
-> PulseForge AudioProcessor
-> selected real output sink
```

The backend creates a virtual sink with `pactl`, makes it the default sink while
enhancement is enabled, and starts `AudioProcessor` to capture from the monitor
stream and play processed audio to the selected PipeWire sink.

If the in-process processor cannot start, the backend may fall back to a simple
loopback path where supported. DSP should always be implemented in C++ inside
PulseForge rather than through external plugin chains.

## Real-Time Audio Rules

Audio callbacks are on the real-time path. Keep them boring and predictable:

- No heap allocations in callbacks.
- No logging in callbacks.
- No locks or blocking calls in callbacks.
- Do not call external processes from callbacks.
- Preserve stereo frame alignment.
- Use atomics, double-buffered configs, or preallocated buffers for live updates.
- Drop or pad whole frames on overrun/underrun, never individual samples.

`SpscFrameRingBuffer` is frame-based: the capture side is the producer, and the
playback side is the consumer. Producer code must not mutate the read index, and
consumer code must not mutate the write index.

## DSP Flow

`AudioProcessor::setEffectChain()` maps preset/effect data into the DSP engine.
The audio callback processes roughly in this order:

```text
input frames
-> preamp/gain
-> parametric EQ
-> stereo width
-> compressor/limiter
-> output clamp
```

DSP components live in `src/dsp/`:

- `ParametricEQ`: RBJ-style peaking EQ bands with double-buffered coefficients.
- `DynamicsProcessor`: simple compressor and limiter.
- `BiquadFilter`: per-channel filter state.
- `SpscFrameRingBuffer`: lock-free frame transport between streams.

When adding DSP features, first define how presets represent the feature, then
map that representation in `AudioProcessor::setEffectChain()`. Keep processing
state preallocated and callback-safe.

## Presets

Built-in presets are defined in `src/presets/PresetFactory.cpp`.

Preset data currently maps to:

- EQ bands and editable band center frequencies.
- Side controls: Fidelity, Ambience, 3D Surround, Dynamic Boost, Bass.
- Preamp and limiter ceiling.
- Compressor settings where useful.
- Stereo width through the `stereo_width` effect.

User presets are saved through `PresetStore` using Qt settings. Keep backward
compatibility when adding stored fields: old presets should load with sensible
defaults.

## GUI Guidelines

The GUI is Qt Widgets with QSS styling. Prefer reusable components in
`src/gui/components/` and keep `MainWindow` focused on orchestration.

When changing UI:

- Use object-name based QSS rather than inline styles.
- Keep spacing generous enough for translated or longer text.
- Avoid nested cards and clutter.
- Keep the main enhancement action obvious.
- Preserve live updates: UI changes should call the service/preset path rather
  than duplicating DSP logic in widgets.

## Backend Guidelines

Public backend methods may lock with the backend mutex. Private helpers should
avoid recursive locking and should be clear about ownership/lifecycle.

Process execution should go through `ProcessRunner` so timeouts and cleanup are
consistent. Cleanup should be best-effort and should not leave the default sink
pointing at a broken PulseForge sink.

## Validation Checklist

Before opening a pull request:

```bash
cmake --build build
```

When available, also run:

```bash
desktop-file-validate data/pulseforge.desktop
appstreamcli validate --no-net data/metainfo/io.github.Moe_Ara.PulseForge.metainfo.xml
```

For audio changes, manually check:

- Enable/disable cycles restore the previous default sink.
- Selected output routing is respected.
- No stale PulseForge modules remain after disable or crash recovery.
- EQ/preset/side-control changes apply live.
- No obvious underruns, crackles, or silence under normal playback.

## Contribution Style

Keep patches focused. A UI redesign, DSP change, and backend lifecycle refactor
should usually be separate changes unless one directly depends on another.

Use clear names for audio units and ranges, for example `preampDb`,
`limiterCeilingDb`, `sampleRate`, and `frames`. Prefer centralized config in
`AudioConfig`, `DspConfig`, and `AppConfig` over new hardcoded constants.
