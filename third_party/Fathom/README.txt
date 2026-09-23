ChessZero Fathom source lock

Fathom is pinned to commit:
c6cf6e8f2f4275e91e03c3612b8d17fe64253c5e

The full upstream source files are required for CHESSZERO_FATHOM=1. This
workspace keeps the upstream LICENSE and a SOURCE.lock manifest even when the
source payload is not present in the current transfer. Use tools/vendor_fathom.sh
to populate the exact pinned tree on a networked machine, or copy an already
verified checkout into this directory.

Offline builds are intentionally fail-fast when src/tbprobe.c is missing.
Do not replace the real Fathom source with a test stub in release builds.
