# v1.1.0 — Full Threads/Hash Runtime Matrix

## Scope

This change-set completes the Android/Chessis runtime reconfiguration test
coverage for the device-facing profiles discussed during development.

### Threads

1, 2, 3, 4, 5, 6, 7.

### Hash

16, 32, 64, 128, 256 MB.

## Runtime behavior

`setoption name Threads value N` and `setoption name Hash value M` are applied
transactionally when a search is active:

1. request a safe stop;
2. wait for the active search/worker pool to quiesce;
3. apply the requested Threads/Hash layout;
4. do not emit the cancelled search's stale `bestmove`;
5. the next `go` uses the new configuration.

Hash remains a total budget for the configured search contexts, not
Hash-times-Threads.

## Coverage added

- 35 static Threads × Hash combinations (7 × 5).
- Threads walk 1 → 7 for every Hash profile.
- Hash walk 16 → 256 → 16 for every Threads value.
- Search smoke test at every Threads value on the 16 MB profile.
- UCI active-search test covering every Threads value.
- UCI active-search test covering every Hash profile.
- UCI mixed sequence changing both dimensions repeatedly while search is active.

## Validation

Host validation completed successfully:

- `v112_runtime_resource`: PASS
- `v113_low_hash_profiles`: PASS
- `v115_engine_api_runtime_reconfigure`: PASS
- `v116_runtime_reconfigure_matrix`: PASS
- `v116_uci_runtime_reconfigure_matrix_test.py`: PASS

This validates the protocol/API/runtime behavior on the host. It does not by
itself replace a final Android/Chessis device test.
