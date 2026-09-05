// Finds functions that reference strings matching one or more case-insensitive terms.
//@category WiiU

import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class FindStringReferences extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException(
                "Usage: FindStringReferences.java <output.json> <term> [term ...]");
        }

        File output = new File(args[0]);
        File parent = output.getAbsoluteFile().getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("Could not create output directory: " + parent);
        }

        List<String> terms = new ArrayList<>();
        for (int i = 1; i < args.length; i++) {
            terms.add(args[i].toLowerCase(Locale.ROOT));
        }

        JsonArray matches = new JsonArray();
        DataIterator dataIterator = currentProgram.getListing().getDefinedData(true);
        while (dataIterator.hasNext() && !monitor.isCancelled()) {
            Data data = dataIterator.next();
            Object value = data.getValue();
            if (!(value instanceof String)) {
                continue;
            }

            String stringValue = (String) value;
            String normalized = stringValue.toLowerCase(Locale.ROOT);
            boolean matched = terms.stream().anyMatch(normalized::contains);
            if (!matched) {
                continue;
            }

            JsonObject item = new JsonObject();
            item.addProperty("address", data.getAddress().toString());
            item.addProperty("value", stringValue);
            JsonArray references = new JsonArray();
            ReferenceIterator iterator =
                currentProgram.getReferenceManager().getReferencesTo(data.getAddress());
            while (iterator.hasNext()) {
                Reference reference = iterator.next();
                JsonObject record = new JsonObject();
                record.addProperty("from", reference.getFromAddress().toString());
                record.addProperty("type", reference.getReferenceType().toString());
                Function function = getFunctionContaining(reference.getFromAddress());
                if (function != null) {
                    record.addProperty("function", function.getName());
                    record.addProperty("functionEntry", function.getEntryPoint().toString());
                }
                references.add(record);
            }
            item.add("references", references);
            matches.add(item);
        }

        JsonObject root = new JsonObject();
        root.addProperty("schemaVersion", 1);
        root.addProperty("program", currentProgram.getName());
        root.add("terms", new Gson().toJsonTree(terms));
        root.add("matches", matches);

        Gson gson = new GsonBuilder().setPrettyPrinting().create();
        try (FileWriter writer = new FileWriter(output)) {
            gson.toJson(root, writer);
        }
        println("Found " + matches.size() + " matching strings in " + output);
    }
}
