# Development status

Updated: 2026-09-05

## Proven on hardware

- Target: USA Wii U Menu `0005001010040100`, version 277.
- Exact `men.rpx` SHA-256:
  `b67deb8fb36889fd5451b89a5b1ce06b31abe341960c3bf1df84e9df4bbe1474`.
- 400 visible titles load across 27 navigable panels.
- The first 26 panels are full; panel 27 contains 10 titles.
- A title on panel 27 launches, and returning to all 27 panels succeeds.
- Raw patches reapply correctly when `men.rpx` reloads; diagnostic hooks are
  registered once and reused.

## 400-title test

The test set contains 378 mock WUHB titles plus 22 existing Menu entries.
`rc16` applied 86/86 checked instructions and reached the Menu, but launching
a title triggered assertion code 77 because `FUN_02274a0c` independently
accepted at most 24 pages. `rc17` raises that validator to 27 pages.

On hardware, the patch released as `v0.1.0` displayed all 400 titles across 27
navigable panels. A title on panel 27 launched successfully, and returning to
the Menu restored all 27 panels. The stock `PageMany` resource still exposes
only 24 cosmetic marker children; bounds-safe patches prevent pages 25-27 from
indexing nonexistent markers.

During one return-to-Menu cycle, the first three panels temporarily omitted
native title icons while mock titles remained. Rebooting restored every icon.
This transient refresh behavior is not yet explained and is a priority for
real-title testing.

## Current limitations

- Only the exact USA v277 binary above is supported.
- The page indicator has only 24 visible markers even though all 27 panels are
  navigable.
- The launch resolver contains two 360-record tables. The tested panel-27 title
  launches successfully, but the tables' purpose and behavior remain to be
  characterized before increasing the capacity again.
- Folder persistence, Data Management, Quick Start, and long-run icon-cache
  behavior above 360 remain unproven.
- `v0.1.0` is an experimental release; it is not declared production-safe.

## Next hardware tests

1. Verify folder creation, movement, and persistence with titles above 360.
2. Exercise Data Management and Quick Start with the 400-title layout.
3. Reboot repeatedly and run longer icon-cache/navigation sessions.
4. Test slots 401-405 before attempting another capacity increase.
