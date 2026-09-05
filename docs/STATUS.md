# Development status

Updated: 2026-09-05

## Proven on hardware

- Target: USA Wii U Menu `0005001010040100`, version 277.
- Exact `men.rpx` SHA-256:
  `b67deb8fb36889fd5451b89a5b1ce06b31abe341960c3bf1df84e9df4bbe1474`.
- `v0.2.0` supports 810 slots / 54 navigable panels.
- 801 visible items loaded: panels 1-53 were full and panel 54 held six.
- A title launched and the complete Menu returned without an assertion or
  panic.
- The 24 physical page markers remain centered and represent normalized
  progress across all 54 pages.

## 800-title-class test

The successful test used 779 mock WUHB titles plus 22 other visible Menu
items. Earlier 810-slot candidates incorrectly extended loops that import and
export Nintendo's fixed 360-record account-save arrays. This read and wrote
through adjacent save fields, producing empty panels, duplicated Disc/folder
icons, launch assertion 401, and a fatal garbage layout record at index 721.

`v0.2.0` keeps live vectors and reconciliation at 810 while preserving the
physical save arrays at 360 records. Entries above 360 are rebuilt through the
normal title reconciliation path. The stock `PageMany` resource has only 24
marker children, so its normalized position is displayed as a compressed,
centered 24-dot bar.

## Current limitations

- Only the exact USA v277 binary above is supported.
- Return-to-Menu reconstruction is noticeably slow with roughly 800 entries.
- Extended layout persistence, folders, Data Management, Quick Start, and
  long-running icon-cache behavior require broader real-title testing.
- The 24-dot indicator represents progress, not one dot per page.

## Next tests

1. Measure and improve return-to-Menu time with 500-800 real titles.
2. Verify folder creation, movement, and persistence above entry 360.
3. Exercise Data Management, Quick Start, repeated reboots, and long sessions.
4. Port the checked patch set to other regional Menu binaries.
