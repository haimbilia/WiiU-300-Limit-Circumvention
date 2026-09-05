# Wii U 300-Title Limit Circumvention

An experimental Aroma/WUPS plugin that extends the stock Wii U Menu instead of
replacing it. The goal is to keep Nintendo's normal Menu, folders, icons, title
launching, and persistence while allowing more than 300 visible entries.

## Current status

This is active hardware research, not a release build.

- 360 titles are confirmed working across 24 full panels on a real Wii U.
- Launching a title and returning to the Menu works at 360 titles.
- The current branch extends the internal model to 405 slots / 27 pages.
- A 400-title boot reaches the Menu, but the stock UI exposes only 24 page
  markers. The logical page count is 27.
- `v1.0.0-rc17` raises the launch-state page validator from 24 to 27 and is
  awaiting hardware results.

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

1. Determine whether pages 25-27 are navigable even though the stock
   `PageMany` resource contains only 24 marker children.
2. Extend the title-launch resolver's two 360-record tables so titles 361-405
   can be launched safely.
3. Test folders, layout persistence, Data Management, and launch/return paths
   above title 360.
4. Port the exact checked patch set to other regional Menu binaries.

See [docs/STATUS.md](docs/STATUS.md) for the hardware evidence and
[docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md) for the important
code paths. Nintendo binaries, keys, tickets, logs, and console-specific data
must never be committed.

## License

BSD 2-Clause. See [LICENSE](LICENSE).
