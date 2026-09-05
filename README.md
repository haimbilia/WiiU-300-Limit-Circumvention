# Wii U 300-Title Limit Circumvention

An experimental Aroma/WUPS plugin that extends the stock Wii U Menu instead of
replacing it, allowing more than 300 visible entries while retaining normal
icons, folders, navigation, and title launching.

## Current status

`v0.2.1` expands the Menu model to 810 slots / 54 pages.

- Verified on hardware with 801 visible items: 53 full panels and six items on
  panel 54.
- A title launches and the complete Menu returns successfully.
- The stock 24-dot bar is centered and acts as a compressed progress indicator
  over all 54 pages, advancing about once every two or three pages.
- Fixes a v0.2.0 state mismatch that could freeze the Menu while scrolling.
- Returning to the Menu can take noticeably longer while hundreds of entries
  and icons are reconstructed.

Only this exact Menu executable is supported:

```text
Region/title: USA 0005001010040100
Version:      277
men.rpx SHA:  b67deb8fb36889fd5451b89a5b1ce06b31abe341960c3bf1df84e9df4bbe1474
```

Every instruction is checked before modification. Other Menu versions fail
closed and remain unpatched.

## Install

Download `WiiUMenuTitleLimit-v0.2.1.wps` from the
[v0.2.1 release](https://github.com/haimbilia/WiiU-300-Limit-Circumvention/releases/tag/v0.2.1)
and copy it to:

```text
sd:/wiiu/environments/aroma/plugins/WiiUMenuTitleLimit.wps
```

Keep direct SD access available, reboot the console, and report your Menu
region/version, real title count, visible panel count, and
`WiiUMenuTitleLimit-status.log` when filing an issue.

## Build

Requirements: Windows PowerShell, Git, and Docker Desktop.

```powershell
git clone https://github.com/haimbilia/WiiU-300-Limit-Circumvention.git
Set-Location WiiU-300-Limit-Circumvention
./scripts/bootstrap-build.ps1
```

Provide your own legally dumped matching `men.rpx` at
`private-dumps/menu/men.rpx`, then run:

```powershell
./scripts/build-plugin.ps1 -Clean -Stage Production -AcknowledgeHardwareRisk
```

The artifact is written to
`plugin/lab/WiiUMenuTitleLimit-production-candidate-unsafe.wps`.

## Known limitations and help wanted

- Only USA Menu v277 is supported.
- The stock account-save layout stores 360 records. Entries beyond that are
  safely rebuilt instead of writing past Nintendo's fixed save arrays, so
  large Menus return more slowly and extended-position persistence needs more
  testing.
- Folder behavior, Data Management, Quick Start, and long-running use with
  hundreds of real installed titles need broader hardware coverage.
- Ports require independently verified patch profiles for other regions and
  Menu versions.

See [docs/STATUS.md](docs/STATUS.md) for hardware evidence and
[docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md) for the relevant
code paths. Never commit Nintendo binaries, keys, tickets, logs, or
console-specific data.

## License

BSD 2-Clause. See [LICENSE](LICENSE).
