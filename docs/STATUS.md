# Development status

Updated: 2026-09-05

## Proven on hardware

- Target: USA Wii U Menu `0005001010040100`, version 277.
- Exact `men.rpx` SHA-256:
  `b67deb8fb36889fd5451b89a5b1ce06b31abe341960c3bf1df84e9df4bbe1474`.
- 360 visible titles load across 24 full panels.
- A tile at the old boundary launches, and returning to the Menu succeeds.
- Raw patches reapply correctly when `men.rpx` reloads; diagnostic hooks are
  registered once and reused.

## 400-title test

The test set contains 378 mock WUHB titles plus 22 existing Menu entries.
`rc16` applied 86/86 checked instructions and reached the Menu without a boot
panic. The Menu model reports 27 pages, but the stock `PageMany` UI resource
contains only 24 child markers, so only 24 markers are visible.

Launching a title under rc16 triggered assertion code 77 because
`FUN_02274a0c` independently accepts at most 24 pages. `rc17` raises this
validator to 27 pages and is awaiting hardware results.

## Current limitations

- Only the exact USA v277 binary above is supported.
- Pages 25-27 may be logically present but still need a navigation test beyond
  the 24 visible markers.
- The launch resolver contains two 360-record tables and must be extended or
  virtualized before titles 361-405 can be considered launch-safe.
- Folder persistence, Data Management, Quick Start, and long-run icon-cache
  behavior above 360 remain unproven.
- No binary is published as a release.

## Next hardware test

1. Boot `v1.0.0-rc17` with 400 titles.
2. Press the right arrow past the final visible page marker and check whether
   pages 25-27 render.
3. Launch a title within the first 360 entries, return to Menu, and capture the
   plugin status log if it hangs.
4. Do not claim titles 361-400 are launch-safe until the resolver tables are
   addressed.
