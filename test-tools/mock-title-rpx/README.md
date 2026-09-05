# SD-only mock titles

`scripts/build-mock-titles.ps1` compiles one harmless game-class WUHB and creates
up to 300 identical copies with unique filenames. Homebrew on Menu derives its
synthetic title ID from each relative SD path, so every copy appears as a
distinct title. The bundles contain both TV and GamePad splash images, which is
the condition Homebrew on Menu uses to classify them as games rather than raw
system-app entries.

The files are not installed to NAND or USB. The deploy script confines them to:

```text
sd:/wiiu/apps/title-limit-test
```

Build and install:

```powershell
.\scripts\build-mock-titles.ps1 -Count 300
.\scripts\deploy-mock-titles.ps1 -Action Install
```

Remove all test titles:

```powershell
.\scripts\deploy-mock-titles.ps1 -Action Remove
```

Reboot after installation or removal because Homebrew on Menu caches its scan.
