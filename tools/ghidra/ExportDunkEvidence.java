// Ghidra headless report for the recovered close-rim dunk presentation chain.
// @category DoubleDribble

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

public class ExportDunkEvidence extends GhidraScript {
    private static final long[] FIXED = {
        0xD40FL, 0xD411L, 0xD428L, 0xD5F9L, 0xD600L, 0xD60CL
    };
    private static final long[] BANK2 = {
        0x8000L, 0x808EL, 0x80C5L, 0x80D4L, 0x80E4L, 0x80F3L,
        0x8104L, 0x8117L, 0x8154L, 0x8163L, 0x81C9L, 0x81E4L,
        0x820FL, 0x8224L, 0x8239L, 0x8245L, 0x825CL, 0x8266L,
        0x827BL, 0x8287L
    };

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException("Expected fixed|bank2 and report path");
        long[] anchors = args[0].equals("fixed") ? FIXED : BANK2;
        File output = new File(args[1]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();
        for (long value : anchors) {
            Address address = toAddr(value);
            if (getInstructionAt(address) == null) disassemble(address);
            if (getFunctionContaining(address) == null && getInstructionAt(address) != null) {
                createFunction(address, String.format("dunk_%s_%04X", args[0], value));
            }
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter report = new PrintWriter(output, "UTF-8")) {
            report.println("Double Dribble Rev 1 - close-rim dunk evidence");
            report.println("Dynamic FCEUX control: ball $01B4/$57, shot edge frame 2614.");
            report.println("Dunk-only fixed PCs include $D40F-$D428 and $D5F9-$D60C; the switched presentation executes bank 2 $8000-$8287.");
            report.println("The native port retains the close-rim eligibility, held-ball rise, and make/miss return to the normal score/loose-ball dispatchers; mapper writes are excluded.");
            report.println();
            for (long value : anchors) {
                Address address = toAddr(value);
                Function function = getFunctionContaining(address);
                report.printf("==== $%04X ==== Function: %s%n", value,
                    function == null ? "<unresolved>" : function.getName());
                for (Reference reference : getReferencesTo(address)) {
                    report.printf("Reference: %s (%s)%n", reference.getFromAddress(), reference.getReferenceType());
                }
                Instruction instruction = getInstructionAt(address);
                for (int count = 0; instruction != null && count < 96; ++count) {
                    report.printf("%s  %s%n", instruction.getAddress(), instruction);
                    instruction = instruction.getNext();
                }
                if (function != null) {
                    DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                    if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                        report.println("-- decompiler --");
                        report.println(result.getDecompiledFunction().getC());
                    }
                }
                report.println();
            }
        } finally {
            decompiler.dispose();
        }
    }
}
