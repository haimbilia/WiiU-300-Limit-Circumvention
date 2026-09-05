// Exports decompiler, disassembly, call-graph, and reference context for selected functions.
//@category WiiU

import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

public class ExportFunctionContext extends GhidraScript {
    private static final int DECOMPILE_TIMEOUT_SECONDS = 120;

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException(
                "Usage: ExportFunctionContext.java <output.json> <address> [address ...]");
        }

        File output = new File(args[0]);
        File parent = output.getAbsoluteFile().getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("Could not create output directory: " + parent);
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(false);
        decompiler.setSimplificationStyle("decompile");
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Could not initialize the decompiler");
        }

        JsonObject root = new JsonObject();
        root.addProperty("schemaVersion", 1);
        root.addProperty("program", currentProgram.getName());
        root.addProperty("language", currentProgram.getLanguageID().toString());
        JsonArray memoryBlocks = new JsonArray();
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            JsonObject record = new JsonObject();
            record.addProperty("name", block.getName());
            record.addProperty("start", block.getStart().toString());
            record.addProperty("end", block.getEnd().toString());
            record.addProperty("read", block.isRead());
            record.addProperty("write", block.isWrite());
            record.addProperty("execute", block.isExecute());
            record.addProperty("initialized", block.isInitialized());
            memoryBlocks.add(record);
        }
        root.add("memoryBlocks", memoryBlocks);
        JsonArray functions = new JsonArray();
        Set<Address> emitted = new HashSet<>();

        try {
            for (int i = 1; i < args.length && !monitor.isCancelled(); i++) {
                Address requested = toAddr(args[i]);
                if (requested == null) {
                    println("Invalid address: " + args[i]);
                    continue;
                }
                Function function = getFunctionAt(requested);
                if (function == null) {
                    function = getFunctionContaining(requested);
                }
                if (function == null) {
                    println("No function at " + requested);
                    continue;
                }
                if (emitted.add(function.getEntryPoint())) {
                    functions.add(exportFunction(function, decompiler));
                }
            }
        } finally {
            decompiler.dispose();
        }

        root.add("functions", functions);
        Gson gson = new GsonBuilder().setPrettyPrinting().create();
        try (FileWriter writer = new FileWriter(output)) {
            gson.toJson(root, writer);
        }
        println("Exported " + functions.size() + " functions to " + output);
    }

    private JsonObject exportFunction(Function function, DecompInterface decompiler) {
        JsonObject item = new JsonObject();
        item.addProperty("name", function.getName());
        item.addProperty("entry", function.getEntryPoint().toString());
        item.addProperty("bodyMin", function.getBody().getMinAddress().toString());
        item.addProperty("bodyMax", function.getBody().getMaxAddress().toString());
        item.addProperty("parameterCount", function.getParameterCount());
        item.add("callers", functionSet(function.getCallingFunctions(monitor)));
        item.add("callees", functionSet(function.getCalledFunctions(monitor)));

        JsonArray instructions = new JsonArray();
        JsonArray references = new JsonArray();
        Set<String> seenReferences = new HashSet<>();
        InstructionIterator iterator =
            currentProgram.getListing().getInstructions(function.getBody(), true);
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = iterator.next();
            JsonObject instructionRecord = new JsonObject();
            instructionRecord.addProperty("address", instruction.getAddress().toString());
            instructionRecord.addProperty("text", instruction.toString());
            try {
                instructionRecord.addProperty("bytes", toHex(instruction.getBytes()));
            } catch (Exception error) {
                instructionRecord.addProperty("bytesError", error.getMessage());
            }
            instructions.add(instructionRecord);

            for (Reference reference : instruction.getReferencesFrom()) {
                String key = reference.getFromAddress() + ":" + reference.getToAddress();
                if (!seenReferences.add(key)) {
                    continue;
                }
                JsonObject referenceRecord = new JsonObject();
                referenceRecord.addProperty("from", reference.getFromAddress().toString());
                referenceRecord.addProperty("to", reference.getToAddress().toString());
                referenceRecord.addProperty("type", reference.getReferenceType().toString());
                Symbol symbol = getSymbolAt(reference.getToAddress());
                if (symbol != null) {
                    referenceRecord.addProperty("symbol", symbol.getName(true));
                }
                Data data = getDataAt(reference.getToAddress());
                if (data != null) {
                    DataType type = data.getDataType();
                    referenceRecord.addProperty("dataType", type.getDisplayName());
                    Object value = data.getValue();
                    if (value instanceof String) {
                        referenceRecord.addProperty("value", (String) value);
                    }
                }
                references.add(referenceRecord);
            }
        }
        item.add("instructions", instructions);
        item.add("references", references);

        DecompileResults results =
            decompiler.decompileFunction(function, DECOMPILE_TIMEOUT_SECONDS, monitor);
        item.addProperty("decompileCompleted", results.decompileCompleted());
        if (results.getErrorMessage() != null && !results.getErrorMessage().isBlank()) {
            item.addProperty("decompileError", results.getErrorMessage());
        }
        if (results.getDecompiledFunction() != null) {
            item.addProperty("decompiledC", results.getDecompiledFunction().getC());
        }
        return item;
    }

    private String toHex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            result.append(String.format("%02x", value & 0xff));
        }
        return result.toString();
    }

    private JsonArray functionSet(Set<Function> functions) {
        List<Function> ordered = new ArrayList<>(functions);
        ordered.sort(Comparator.comparing(function -> function.getEntryPoint().toString()));
        JsonArray result = new JsonArray();
        for (Function function : ordered) {
            JsonObject item = new JsonObject();
            item.addProperty("name", function.getName());
            item.addProperty("entry", function.getEntryPoint().toString());
            result.add(item);
        }
        return result;
    }
}
