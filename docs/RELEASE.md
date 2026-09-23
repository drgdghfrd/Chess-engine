# ChessZero v1.0 Release Notes

## What is frozen

v1.0 is the stable continuation of the v0.99 RC. The asynchronous UCI worker,
clock-management controls, Lazy SMP/shared-TT baseline, PolyGlot book path and
HalfKP evaluation path are carried forward without a new search heuristic in the
release step.

## Platform scope

| Platform | Release support in this source package |
|---|---|
| Linux | Native CMake build |
| Windows | Native CMake/Visual Studio build configuration |
| Android | NDK native shared-library/OEX-compatible build |

The Android target is an engine library rather than a finished APK. A UI shell and
application packaging layer must be supplied by the host Android application.

## Verification

Release verification is defined by deterministic engine tests plus an interactive
UCI protocol test. Build logs are environment-specific; the release archive itself
contains no transient build directory.

No absolute Elo number is published by v1.0 because this repository does not include
an independently verified match result for a fixed hardware/time-control reference
at release time.


## v1.0.2 benchmark-preparation patch

The cross-engine match harness was corrected to ignore informational UCI lines until
`bestmove` arrives, and position-state handling now rejects attempts to capture a king.
No Elo result is published by this patch.
