# Ghidra helpers

These scripts operate on a contributor's own Wii U Menu dump. Add this
directory to Ghidra's script paths after importing `men.rpx` with an RPX loader.

- `FindTitleLimitCandidates.java` ranks functions containing capacity-related
  constants and title-list references.
- `ExportFunctionContext.java` exports focused disassembly, decompiler output,
  references, callers, and callees as JSON.
- `FindStringReferences.java` finds code references to Menu resource strings.
- `ListFunctionsInRange.java` inventories functions in an address range.
- `FixPrimaryImports.java` repairs imported-reference metadata before analysis.

Generated projects and reports are intentionally ignored because they derive
from copyrighted console binaries. Commit only minimal addresses, instruction
words, and independently written analysis notes.
