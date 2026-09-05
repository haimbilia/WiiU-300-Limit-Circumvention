// Lists function entries in an inclusive address range.
//@category WiiU

import java.io.File;
import java.io.FileWriter;

import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ListFunctionsInRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "Usage: ListFunctionsInRange.java <output.json> <start> <end>");
        }

        File output = new File(args[0]);
        File parent = output.getAbsoluteFile().getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("Could not create output directory: " + parent);
        }

        Address start = toAddr(args[1]);
        Address end = toAddr(args[2]);
        if (start == null || end == null || start.compareTo(end) > 0) {
            throw new IllegalArgumentException("Invalid function range");
        }

        JsonArray functions = new JsonArray();
        FunctionIterator iterator = currentProgram.getFunctionManager()
            .getFunctions(new AddressSet(start, end), true);
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Function function = iterator.next();
            JsonObject item = new JsonObject();
            item.addProperty("name", function.getName());
            item.addProperty("entry", function.getEntryPoint().toString());
            item.addProperty("bodyMin", function.getBody().getMinAddress().toString());
            item.addProperty("bodyMax", function.getBody().getMaxAddress().toString());
            functions.add(item);
        }

        JsonObject root = new JsonObject();
        root.addProperty("schemaVersion", 1);
        root.addProperty("program", currentProgram.getName());
        root.addProperty("start", start.toString());
        root.addProperty("end", end.toString());
        root.add("functions", functions);

        try (FileWriter writer = new FileWriter(output)) {
            new GsonBuilder().setPrettyPrinting().create().toJson(root, writer);
        }
        println("Listed " + functions.size() + " functions in " + output);
    }
}
