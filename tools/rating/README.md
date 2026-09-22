# Engine rating tools

This package is for reproducible engine-vs-engine measurement.

## Current output

Given W/D/L, the helper reports:

- score percentage;
- Elo difference between engine A and engine B;
- an approximate 95% confidence interval;
- the interval method used.

The interval is a Wilson interval applied to the half-point score fraction. Because chess results are ternary (win/draw/loss), this is an approximation and should be described as such in published results.

## Example

```bash
PYTHONPATH=tools python3 -m rating.rating --wins 110 --draws 40 --losses 50
```

For a stronger benchmark protocol, pair this with fixed openings, alternating colors, a fixed time control, recorded PGN, engine options, hardware metadata, and a reproducible engine binary/commit.
