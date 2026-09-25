ChessZero Fathom vendored-source lock

Fathom provenance is pinned to commit:
c6cf6e8f2f4275e91e03c3612b8d17fe64253c5e

The source package carries a checked-in vendored snapshot plus SOURCE.lock.
The lock records the exact Git blob SHA-1 and size of the vendored files so the
offline Android build is self-contained. tools/vendor_fathom.sh can replace the
snapshot from the referenced upstream commit on a networked machine and regenerate
the local lock.

Offline builds are intentionally fail-fast when src/tbprobe.c is missing.
Do not replace the real Fathom source with a test stub in release builds.
