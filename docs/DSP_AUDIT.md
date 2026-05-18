# PulseForge DSP Audit

This document records the current DSP tuning direction and the main issues found
during the clarity/presence cleanup pass.

## Findings

- Gain staging was too easy to overload when EQ, side controls, preamp, and
  Dynamic Boost stacked together.
- The original limiter behaved like an instant sample clamp. It prevented some
  numeric clipping, but heavy activation could sound crackly or flattened.
- EQ bands used the same Q everywhere, which made low bass, low mids, presence,
  and air bands feel less musical than they should.
- Dynamic Boost could feel reversed because it increased compression strength
  enough to reduce perceived level.
- Wide stereo settings could increase side-channel peaks and make the limiter
  work harder.
- Extreme GUI settings need guardrails; PulseForge should get cleaner or safer
  at extremes, not louder and broken.

## Current DSP Order

```text
input
-> smoothed preamp/gain
-> parametric EQ with smoothed coefficient updates
-> smoothed mid/side stereo width
-> compressor
-> smoothed limiter / soft safety shaping
-> output clamp
```

## Tuning Principles

- Cut mud before boosting presence.
- Prefer broad, low-Q curves for musical enhancement.
- Keep 3-6 kHz boosts controlled to avoid fatigue.
- Treat bass boost as low-bass warmth plus low-mid cleanup, not just more
  200-400 Hz.
- Use automatic headroom for stacked boosts.
- Keep limiter activation occasional; constant limiting means the preset or
  user curve is too hot.

## Safety Guardrails

- Effective EQ boosts are bounded before they reach the processor.
- Total positive EQ boost is scaled when stacked boosts become excessive.
- Automatic EQ headroom increases with total and peak boost.
- Preamp and stereo width are smoothed during live updates.
- Limiter release is smoothed to reduce crackle from rapid gain changes.
- Meter snapshots log peak, limiter activation count, and clipped sample count
  outside the audio callback when DSP settings are updated.

## Preset Intent

- `Flat`: almost neutral with slight cleanup and safe headroom.
- `Gaming`: positional clarity without excessive bass.
- `Music`: gentle bass warmth, low-mid cleanup, and controlled air.
- `Movie`: fuller lows and dialogue presence.
- `Voice`: rumble reduction and speech intelligibility.
- `Bass Boost`: stronger lows with low-mid control.
- `Clarity`: presence and detail without sharp upper mids.
- `Competitive FPS`: footstep and positional focus with minimal low-end masking.
- `Warm`: relaxed listening with smoother highs.
- `Bright`: air/detail with restrained harshness.

## Future Work

- Add true lookahead limiting if latency budget allows.
- Add optional loudness normalization or replay-gain style target matching.
- Add GUI-visible peak/limiter indicators.
- Add offline impulse/sine sweep tests for EQ stability.
- Add automated tests for NaN/Inf rejection and coefficient bounds.
