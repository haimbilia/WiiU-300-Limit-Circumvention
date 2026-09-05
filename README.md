# Wii U 300-Title Limit Circumvention

An experimental Aroma/WUPS plugin that extends the stock Wii U Menu instead of
replacing it. The goal is to keep Nintendo's normal Menu, folders, icons, title
launching, and persistence while allowing more than 300 visible entries.

## Current status

This is active hardware research, not a release build.

- `v1.0.0-rc17` is confirmed working with 400 visible titles on real hardware.
- All 27 panels are navigable: the first 26 are full and the last has 10 titles.
- A title on panel 27 launches, and returning to the complete Menu succeeds.
- The internal model supports 405 slots / 27 pages. The cosmetic page indicator
  still has only its stock 24 markers.

The patch currently supports only this exact Menu executable:

```text
Region/title: USA 0005001010040100
Version:      277
men.rpx SHA:  b67deb8fb36889fd5451b89a5b1ce06b31abe341960c3bf1df84e9df4bbe1474
```

Every instruction is checked before modification. Other Menu versions fail
closed and remain unpatched.

## Build

Requirements: Windows PowerShell, Git, and Docker Desktop.

```powershell
git clone https://github.com/haimbilia/WiiU-300-Limit-Circumvention.git
Set-Location WiiU-300-Limit-Circumvention
./scripts/bootstrap-build.ps1
```

For the production candidate, provide your own legally dumped matching
`men.rpx` at `private-dumps/menu/men.rpx`, then run. If a private Ghidra
instruction report is present, the build validates it too; otherwise the
runtime still verifies every original instruction before patching.

```powershell
./scripts/build-plugin.ps1 -Clean -Stage Production -AcknowledgeHardwareRisk
```

Output:

```text
plugin/lab/WiiUMenuTitleLimit-production-candidate-unsafe.wps
```

Do not install this on a console whose Menu identity differs from the target
above. Keep Aroma safe mode or direct SD access available during testing.

## Help wanted

The most useful next contributions are:

1. Test folders, layout persistence, Data Management, Quick Start, and
   long-running icon-cache behavior above title 360.
2. Test the unoccupied 401-405 slots and larger title sets to locate the next
   coherent capacity boundary.
3. Explain or replace the stock 24-marker page indicator for pages 25-27.
4. Port the exact checked patch set to other regional Menu binaries.

See [docs/STATUS.md](docs/STATUS.md) for the hardware evidence and
[docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md) for the important
code paths. Nintendo binaries, keys, tickets, logs, and console-specific data
must never be committed.

## License

BSD 2-Clause. See [LICENSE](LICENSE).
