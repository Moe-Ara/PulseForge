# PulseForge v0.1.0-alpha Release Checklist

## Build And Packaging

- [ ] Clean source tree, except intentional release changes.
- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- [ ] `cmake --build build --parallel`
- [ ] `cmake --install build --prefix "$HOME/.local"` for a local install smoke test.
- [ ] `scripts/package-appimage.sh` produces `PulseForge-v0.1.0-alpha-x86_64.AppImage`.
- [ ] SHA256 checksum generated beside the AppImage.

## Desktop Integration

- [ ] Desktop file validates with `desktop-file-validate`.
- [ ] AppStream metadata validates with `appstreamcli validate --no-net`.
- [ ] App launches from terminal with `pulseforge`.
- [ ] App launches from the desktop menu.
- [ ] AppImage launches on a clean Fedora/Bazzite user session.
- [ ] Icon appears correctly in the app menu and tray.

## Audio Lifecycle

- [ ] Runtime dependency dialog appears if PipeWire, WirePlumber, pactl, or pipewire-pulse is missing.
- [ ] Device selector lists real PipeWire output devices only.
- [ ] Enable enhancement routes audio through PulseForge.
- [ ] Disable enhancement restores the previous default sink.
- [ ] Quit restores the previous default sink.
- [ ] No stale `pulseforge_enhanced` virtual sinks remain after disable.
- [ ] Crash/restart cleanup removes stale modules and restores defaults.
- [ ] Mic isolation does not leave `pulseforge_enhanced.monitor` as the default source.

## UI And Session Behavior

- [ ] No dead buttons.
- [ ] Layout does not overlap at the minimum supported window size.
- [ ] Close-to-tray works when enabled.
- [ ] Quit from tray fully exits and cleans up audio routing.
- [ ] `--start-minimized` starts hidden in tray.
- [ ] Start-on-login setting enables a user-level systemd service.
- [ ] Start-on-login setting disables that service cleanly.
- [ ] First-run dialog appears once.

## Release Assets

- [ ] AppImage uploaded.
- [ ] `.sha256` checksum uploaded.
- [ ] Source tarball uploaded.
- [ ] Screenshots prepared.
- [ ] README badges and install instructions reviewed.
- [ ] GitHub release notes prepared.
- [ ] Known alpha limitations listed.

## Target Systems

- [ ] Tested on Bazzite.
- [ ] Tested on Fedora Workstation.
- [ ] Tested on at least one non-default output device.
- [ ] Tested with Firefox/Chromium playback.
- [ ] Tested with Discord or another chat app for mic isolation.
