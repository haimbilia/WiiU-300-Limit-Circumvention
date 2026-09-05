# Reverse-engineering notes

## Target and relocation

The current patch profile targets USA Menu v277 with `men.rpx` SHA-256
`b67deb8fb36889fd5451b89a5b1ce06b31abe341960c3bf1df84e9df4bbe1474`.
The canonical executable base is `0x02000000`; the observed hardware runtime
base is `0x0dd00000`. Runtime log addresses therefore map back by subtracting
the observed text offset `0x0bd00000`.

## Capacity chain

The Menu requests thousands of MCP records, then imposes smaller limits in its
own model. The known chain is:

- `FUN_021710cc` owns three 360-entry layout vectors and one 300-entry scratch
  vector.
- `FUN_021736f8` computes 15 slots per page, clamps at 24 pages, and marks the
  Menu full at 300 entries.
- Several downstream stack arrays and record vectors were only 304 entries.
- `FUN_0216436c` owns a separate 360-entry banner/model vector.
- The stock `PageMany` UI resource has 24 marker children. Its initialization
  and per-frame update helpers panic if the logical count exceeds that vector.
- `FUN_02274a0c` separately validates a maximum of 24 pages and searches two
  fixed 360-record launch tables.

The current plugin raises the coherent layout path to 405 slots, dependent
304-entry buffers to 409, page bounds to 27, and prevents the cosmetic marker
widget from indexing beyond its 24 real children. The full checked patch list
and rationale are in `plugin/src/patches/patch_runtime.cpp`.

## Failure sequence that located the boundaries

| Build | Hardware result | Canonical failure |
| --- | --- | --- |
| rc8 | 360-title boot stopped | assertion 429 at `0x02173b90` |
| rc9 | next vector bound | panic at `0x0217a04c` |
| rc10 | BOSS/new-arrival bound | assertion 65 at `0x02169a20` |
| rc11 | 24 full panels rendered | return hook duplication |
| rc12 | 360 boot + launch + return passed | none |
| rc13 | 400 model constructed | page-child init panic at `0x02001fc0` |
| rc14 | init passed | page-child update panic at `0x02001a60` |
| rc15 | child accesses skipped | assertion 108 at `0x02001ad8` |
| rc16 | Menu rendered, launch attempted | assertion 77 at `0x02274a74` |
| rc17 | 400 titles, 27 panels, panel-27 launch + return passed | none |

The assertion and panic hooks log the runtime link register. Always subtract
the actual loaded text offset before opening the canonical address in Ghidra.

## Safety properties

- Exact title ID, title version, and loaded executable bounds are checked.
- Every original instruction word is validated before any write.
- Partial application rolls back already-written instructions.
- Data and instruction caches are synchronized on all three Espresso cores.
- FunctionPatcher hooks are reused across Menu reloads instead of duplicated.
- Unsupported builds remain untouched.

Do not commit Nintendo binaries, decompiler output derived from private dumps,
keys, tickets, or console logs. Public analysis scripts and address-level facts
are sufficient to reproduce the work from a contributor's own dump.
