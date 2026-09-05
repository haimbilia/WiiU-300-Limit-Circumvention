// Finds instruction-level constants and MCP callers related to the Wii U Menu title limit.
//@category WiiU

import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;

public class FindTitleLimitCandidates extends GhidraScript {
    private static final Map<Long, ConstantSpec> CONSTANTS = new LinkedHashMap<>();
    private static final String[] MCP_SYMBOLS = {
        "MCP_TitleCount",
        "MCP_TitleList",
        "MCP_TitleListByAppType",
        "MCP_TitleListByUniqueId",
        "MCP_TitleListByDevice",
        "MCP_TitleListByDeviceType",
        "MCP_TitleListByAppAndDevice",
        "MCP_TitleListByAppAndDeviceType",
        "MCP_TitleListByUniqueIdAndIndexedDeviceAndAppType"
    };

    static {
        addConstant(300, "official-software-icon-limit", 100);
        addConstant(292, "reported-usable-capacity", 45);
        addConstant(600, "first-probe-limit", 35);
        addConstant(60, "folder-or-folder-slot-count", 10);
        addConstant(97, "sizeof-MCPTitleListType", 25);
        addConstant(0x71ac, "300-times-MCPTitleListType", 95);
        addConstant(0x12c0, "300-times-menu-entry-size", 85);
        addConstant(0x2580, "300-times-icon-record-size", 90);
        addConstant(0x25e0, "Barista-icon-database-object-size", 90);
        addConstant(0x1c24, "Barista-icon-database-header-size", 35);
        addConstant(0x2d24, "Barista-folder-table-offset", 45);
        addConstant(3600, "60-times-60-folder-capacity", 40);
        addConstant(3840, "inferred-max-software-placements", 40);
        addConstant(8000, "documented-MCP-title-capacity", 25);
        addConstant(0x100ac, "sizeof-nn-sl-IconInfo", 30);
        addConstant(0x12060, "sizeof-nn-idb-FileEntry", 30);
    }

    private static void addConstant(long value, String label, int score) {
        CONSTANTS.put(value, new ConstantSpec(value, label, score));
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            throw new IllegalArgumentException(
                "Output path required: -postScript FindTitleLimitCandidates.java <output.json> [maxCandidates]");
        }

        File output = new File(args[0]);
        int maxCandidates = args.length >= 2 ? Integer.parseInt(args[1]) : 2000;
        File parent = output.getAbsoluteFile().getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("Could not create output directory: " + parent);
        }

        Map<String, McpCaller> mcpCallers = collectMcpCallers();
        List<Candidate> candidates = collectCandidates(mcpCallers);
        candidates.sort(Comparator
            .comparingInt((Candidate candidate) -> candidate.score).reversed()
            .thenComparing(candidate -> candidate.address)
            .thenComparing(candidate -> candidate.constantHex));

        int totalCandidates = candidates.size();
        if (candidates.size() > maxCandidates) {
            candidates = new ArrayList<>(candidates.subList(0, maxCandidates));
        }

        JsonObject root = new JsonObject();
        root.addProperty("schemaVersion", 1);
        root.addProperty("program", currentProgram.getName());
        root.addProperty("executableFormat", currentProgram.getExecutableFormat());
        root.addProperty("language", currentProgram.getLanguageID().toString());
        root.addProperty("imageBase", currentProgram.getImageBase().toString());
        root.addProperty("minimumAddress", currentProgram.getMinAddress().toString());
        root.addProperty("maximumAddress", currentProgram.getMaxAddress().toString());
        root.addProperty("totalCandidates", totalCandidates);
        root.addProperty("emittedCandidates", candidates.size());

        JsonArray constants = new JsonArray();
        for (ConstantSpec spec : CONSTANTS.values()) {
            JsonObject item = new JsonObject();
            item.addProperty("value", spec.value);
            item.addProperty("hex", String.format("0x%x", spec.value));
            item.addProperty("label", spec.label);
            constants.add(item);
        }
        root.add("searchedConstants", constants);

        Gson gson = new GsonBuilder().setPrettyPrinting().create();
        root.add("mcpCallers", gson.toJsonTree(mcpCallers.values()));
        root.add("candidates", gson.toJsonTree(candidates));

        try (FileWriter writer = new FileWriter(output)) {
            gson.toJson(root, writer);
        }

        println("Wrote " + candidates.size() + " of " + totalCandidates +
            " candidates and " + mcpCallers.size() + " MCP callers to " + output);
    }

    private Map<String, McpCaller> collectMcpCallers() {
        Map<String, McpCaller> callers = new LinkedHashMap<>();
        Set<String> seenReferences = new HashSet<>();

        for (String symbolName : MCP_SYMBOLS) {
            List<Symbol> symbols = getSymbols(symbolName, null);
            for (Symbol symbol : symbols) {
                collectReferences(symbolName, symbol.getAddress(), callers, seenReferences);
                Function target = getFunctionAt(symbol.getAddress());
                if (target != null) {
                    collectReferences(symbolName, target.getEntryPoint(), callers, seenReferences);
                }
            }
        }
        return callers;
    }

    private void collectReferences(String symbolName, Address target,
                                   Map<String, McpCaller> callers,
                                   Set<String> seenReferences) {
        ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(target);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            String referenceKey = symbolName + ":" + reference.getFromAddress();
            if (!seenReferences.add(referenceKey)) {
                continue;
            }

            Function caller = getFunctionContaining(reference.getFromAddress());
            if (caller == null) {
                continue;
            }

            String entry = caller.getEntryPoint().toString();
            McpCaller record = callers.computeIfAbsent(entry,
                ignored -> new McpCaller(caller.getName(), entry));
            record.calledSymbols.add(symbolName);
            record.callSites.add(reference.getFromAddress().toString());
        }
    }

    private List<Candidate> collectCandidates(Map<String, McpCaller> mcpCallers) {
        List<Candidate> candidates = new ArrayList<>();
        Listing listing = currentProgram.getListing();
        InstructionIterator instructions = listing.getInstructions(true);

        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            Set<Long> matchedAtInstruction = new HashSet<>();
            for (int operandIndex = 0; operandIndex < instruction.getNumOperands(); operandIndex++) {
                for (Object object : instruction.getOpObjects(operandIndex)) {
                    if (!(object instanceof Scalar)) {
                        continue;
                    }

                    long value = ((Scalar) object).getUnsignedValue();
                    ConstantSpec spec = CONSTANTS.get(value);
                    if (spec == null || !matchedAtInstruction.add(value)) {
                        continue;
                    }

                    Function function = getFunctionContaining(instruction.getAddress());
                    String functionEntry = function == null ? null : function.getEntryPoint().toString();
                    boolean callsMcp = functionEntry != null && mcpCallers.containsKey(functionEntry);
                    int score = scoreCandidate(spec, instruction.getMnemonicString(), callsMcp);
                    candidates.add(new Candidate(
                        instruction.getAddress().toString(),
                        instruction.getMnemonicString(),
                        instruction.toString(),
                        spec,
                        score,
                        function == null ? null : function.getName(),
                        functionEntry,
                        callsMcp));
                }
            }
        }
        return candidates;
    }

    private int scoreCandidate(ConstantSpec spec, String mnemonic, boolean callsMcp) {
        int score = spec.baseScore;
        String normalized = mnemonic.toLowerCase();
        if (normalized.startsWith("cmp")) {
            score += 70;
        }
        if (normalized.equals("li") || normalized.equals("lis") ||
            normalized.startsWith("addi") || normalized.startsWith("mulli")) {
            score += 15;
        }
        if (callsMcp) {
            score += 60;
        }
        return score;
    }

    private static final class ConstantSpec {
        final long value;
        final String label;
        final int baseScore;

        ConstantSpec(long value, String label, int baseScore) {
            this.value = value;
            this.label = label;
            this.baseScore = baseScore;
        }
    }

    private static final class McpCaller {
        final String function;
        final String entry;
        final Set<String> calledSymbols = new HashSet<>();
        final Set<String> callSites = new HashSet<>();

        McpCaller(String function, String entry) {
            this.function = function;
            this.entry = entry;
        }
    }

    private static final class Candidate {
        final String address;
        final String mnemonic;
        final String instruction;
        final long constant;
        final String constantHex;
        final String label;
        final int score;
        final String function;
        final String functionEntry;
        final boolean functionCallsMcp;

        Candidate(String address, String mnemonic, String instruction,
                  ConstantSpec spec, int score, String function,
                  String functionEntry, boolean functionCallsMcp) {
            this.address = address;
            this.mnemonic = mnemonic;
            this.instruction = instruction;
            this.constant = spec.value;
            this.constantHex = String.format("0x%x", spec.value);
            this.label = spec.label;
            this.score = score;
            this.function = function;
            this.functionEntry = functionEntry;
            this.functionCallsMcp = functionCallsMcp;
        }
    }
}
