# BassDeamon 0.2.1 benchmark — 2026-11-08

Tested code: `1401e355b78050c51b5408a79cef873373cce5cc`.
Frozen 0.2.0 reference: `e6d48b7fa76979da31214eca03d4190fdef3578e`.
[Successful CI run](https://github.com/gio-dev-bio/bassdeamon/actions/runs/42474838355).

## CPU

72 stereo conditions per revision/platform: 44.1/48/96 kHz, buffers 64/128/256/512, six module combinations. Three 0.6-second repeats with 30 warmup blocks. Wall-clock core DSP timings exclude host, GUI and input copies. Baseline and candidate alternate per condition. Native Intel and native ARM runners, Apple clang 17, `-O3 -DNDEBUG`, no fast-math.

| Runner | Median reduction of case mean CPU | Example 48 kHz / 128 / all / AGC off, one core | Example p99 µs |
| --- | ---: | ---: | ---: |
| Intel i7-8700B, macOS 15.7.9 | 30.5% | 3.04% → 2.17% | 188.71 → 74.88 |
| Apple M1 virtual, macOS 15.8.2 | 30.0% | 1.77% → 1.33% | 57.29 → 96.46 |
| Linux shared runner, control | 40.2% | 2.88% → 1.78% | 152.64 → 81.26 |

The example callback budget is 2666.67 µs. ARM p99 worsened in that example despite improving mean; this is not a universal tail-latency improvement. No callback exceeded budget in either native Mac run. Linux contained deadline overruns (2 baseline cases and 3 candidate cases); these shared, non-real-time machines cannot establish hard real-time safety.

Zero C++ new/new[] allocations, including aligned forms, observed in the core callbacks across all cases. This does not intercept every OS malloc. JUCE processBlock allocation checks also passed in the plugin test suite.

## Audio regression

48 kHz float fixtures, coherent 40/80/160 Hz tones at −12.04 dBFS peak for 10 s; analyze last 2 s after fitting fundamental and DC. Dynamic fixtures are not treated as stationary THD+N.

| Module | THD+N 40 Hz | 80 Hz | 160 Hz |
| --- | ---: | ---: | ---: |
| GLUE | −73.01 dB | −84.90 dB | −96.87 dB |
| UNMASK | −121.12 dB | −147.44 dB | −147.39 dB |
| Static PHASE | −150.59 dB | −150.48 dB | −150.61 dB |

GLUE is nonlinear: measured 40 Hz distortion is approximately 0.0224%, not zero. Very low PHASE/UNMASK readings approach the float fixture's numerical floor and are not an analog measurement claim.

- Exact bypass and zero output for exact silence.
- No NaN/Inf in any rendered fixture.
- All-module tail below −120 dBFS after 3 seconds of silence.
- Quiet UNMASK 80 Hz fixture: −99.67 dB THD+N.
- Frozen-reference null tests (240,000 samples): exact for bypass/GLUE/UNMASK/all; AGC maximum error 1.49e−8 FS, Learn 7.45e−9 FS. Width automation intentionally differs.
- Extreme WIDTH 0/100% four changes/s: high-pass >2 kHz peak improves from −67.09 to −83.91 dBFS, **16.82 dB** reduction. Method: sixth-order Butterworth high-pass, exclude first second. Candidate HF RMS −108.91 dBFS.
- Phase delay/polarity automation intentionally changes timing/phase; this is not an artifact-free guarantee for extreme parameter movement.

## Changes and verification

Block-invariant math hoisted; all-pass coefficients cached; EQ coefficient computation shared across channels while keeping filter states independent; exact fast paths for zero delay/unit gain/inactive learner/saturated detector. WIDTH Q smoothing uses 20 ms and preserves initial/static settings. No fast-math approximations. GUI spectrum coefficient calculations reduced from 1932 to 12 per draw, but full GUI CPU is not benchmarked.

DSP unit tests, ASan/UBSan, JUCE processor tests, complete-bundle ad-hoc signature verification, and pluginval 1.0.4 strictness 5 passed. Pluginval ran ARM native and Intel via Rosetta; CPU benchmarks ran on separate native architectures. The universal ZIP is a development build, not notarized or proven in Cubase on Giorgio's Mac.

Reproduce using `tests/Benchmark.cpp`, `benchmarks/analyze.py`, and the optional CMake benchmark target. Raw CSV/JSON and hardware identifiers are in `benchmarks/results/2026-11-08/`. `benchmarks/report.py` generates a self-contained HTML report and comparison WAVs when the raw float renders are available. NumPy 2.2.6/SciPy 1.15.3 are pinned in CI.

Automatic thresholds are regression checks, not audibility certification. Listening tests with real kick/bass, Cubase load, GUI open, multiple instances, and the user's exact Intel Mac remain necessary. No oversampling or limiter is present.
