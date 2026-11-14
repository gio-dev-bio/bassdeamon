# BassDeamon 0.2.2 — finite bypass and conservative PHASE Learn

## Changes

- GUI POWER now crossfades the whole chain to dry in exactly 20 ms (rounded to integer samples). Its endpoint leaves the original input bits untouched, including signed zero and subnormal floats. Fully bypassed DSP skips processing and pauses adaptation; module settings and learned runtime values remain stored. The host bypass path remains immediate and unchanged.
- PHASE delay changes crossfade two fixed delay taps over 20 ms instead of sweeping a moving tap. Rapid commands keep the latest requested delay and apply it after the current fade completes; this bounds tap count and avoids discontinuities from repeatedly replacing a live tap. This trades the moving-tap pitch glide for a short blend which can cause comb filtering/amplitude changes. It is not a promise of transparent delay changes.
- Polarity uses a finite 20 ms ramp. Fading between opposite polarities necessarily passes through cancellation; the host's hard bypass can still click when the two waveforms disagree.
- Learn uses centred correlations to reject constant/DC matches. Acceptance requires |r| ≥0.55, a margin ≥0.08 against alternatives at least 1 ms away, and compatible estimates in the first and last two seconds (same polarity, lag within 0.5 ms, |r| ≥0.55 each). Signal-count/energy gates also apply. These are conservative engineering heuristics, not a calibrated probability or musical quality score.
- GUI distinguishes weak/no signal, ambiguous match and moving match. Rejected estimates do not overwrite delay/polarity. Learn stays manual-triggered, does not enable PHASE, and is disabled while POWER is off.

## Verification

New contracts cover bit-exact finite master bypass, resume, analytic fixed-tap crossfade output, and Learn rejection of unrelated noise, DC, periodic ambiguities and changing lag. The known 2 ms inverted broadband reference must still be recovered. Existing DSP, allocation, mono/stereo/routing/state and signal tests remain in CI. Extended JUCE bypass tests now require exact initial bypass and zero error after settling, and run on separate native Intel and ARM Mac runners.

Development build: ad-hoc signed, not notarized. A real kick/bass listening test and Cubase session on Giorgio's Intel Mac remain necessary. No real audio stems were supplied for this iteration; synthetic signal tests do not substitute for those checks.
