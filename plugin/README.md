# Plugin source

`src/patches/patch_runtime.cpp` contains the checked PowerPC instruction patch
set and diagnostic FunctionPatcher hooks. `src/main.cpp` owns the WUPS
lifecycle.

The plugin activates only for USA Menu `0005001010040100`, version 277, with
the expected executable layout and instruction words. Patches are applied with
`KernelCopyData`, synchronized across all Espresso cores, and rolled back on a
partial failure.

Build from the repository root:

```powershell
./scripts/bootstrap-build.ps1
./scripts/build-plugin.ps1 -Clean -Stage Production -AcknowledgeHardwareRisk
```

This produces an experimental hardware-test artifact under `plugin/lab/`.
There is no supported release binary yet.
