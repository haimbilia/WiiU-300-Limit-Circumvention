// Marks imported references primary before title-limit candidate collection.
//@category WiiU

import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.SourceType;

public class FixPrimaryImports extends GhidraScript {
    @Override
    public void run() throws Exception {
        ReferenceManager referenceManager = currentProgram.getReferenceManager();
        ReferenceIterator references =
            referenceManager.getReferenceIterator(currentProgram.getMinAddress());
        int changed = 0;

        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (reference.getSource() == SourceType.IMPORTED && !reference.isPrimary()) {
                referenceManager.setPrimary(reference, true);
                changed++;
            }
        }

        println("Marked " + changed + " imported references primary");
    }
}
