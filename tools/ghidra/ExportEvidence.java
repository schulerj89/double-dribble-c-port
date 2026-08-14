// Ghidra headless post-script for repeatable Double Dribble evidence exports.
// @category DoubleDribble

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

public class ExportEvidence extends GhidraScript {
    private static final long[] ANCHORS = {
        0xC001L, 0xC036L, 0xC04EL, 0xC08FL, 0xC164L, 0xC187L,
        0xC220L, 0xC230L,
        0xC41CL, 0xC43AL, 0xC566L, 0xC57DL, 0xC597L, 0xC5A9L, 0xC5B4L,
        0xC5C3L, 0xC141L, 0xC724L, 0xC77FL, 0xCBE0L,
        0xCD70L, 0xCD83L, 0xCD8BL, 0xCD96L, 0xCE75L,
        0xD368L, 0xD371L, 0xD393L, 0xD3A0L
    };

    @Override
    public void run() throws Exception {
        if (getScriptArgs().length != 1) {
            throw new IllegalArgumentException("Expected output report path");
        }

        File output = new File(getScriptArgs()[0]);
        File parent = output.getParentFile();
        if (parent != null) {
            parent.mkdirs();
        }

        PseudoDisassembler pseudo = new PseudoDisassembler(currentProgram);
        for (long value : ANCHORS) {
            Address address = toAddr(value);
            if (getInstructionAt(address) == null &&
                (pseudo.isValidSubroutine(address, true) || value == 0xC220L || value == 0xC724L)) {
                disassemble(address);
            }
            if (getFunctionContaining(address) == null && getInstructionAt(address) != null) {
                createFunction(address, String.format("evidence_%04X", value));
            }
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        try (PrintWriter report = new PrintWriter(output, "UTF-8")) {
            report.println("Double Dribble Rev 1 - Ghidra evidence report");
            report.println("Program: " + currentProgram.getName());
            report.println("Language: " + currentProgram.getLanguageID());
            report.println();

            for (long value : ANCHORS) {
                Address address = toAddr(value);
                report.printf("==== $%04X ====%n", value);
                Function function = getFunctionContaining(address);
                report.println("Function: " + (function == null ? "<unresolved>" : function.getName()));
                report.println("References to anchor:");
                Reference[] references = getReferencesTo(address);
                if (references.length == 0) {
                    report.println("  <none>");
                }
                for (Reference reference : references) {
                    report.printf("  %s (%s)%n", reference.getFromAddress(), reference.getReferenceType());
                }

                Instruction instruction = getInstructionAt(address);
                for (int count = 0; instruction != null && count < 32; count++) {
                    report.printf("%s  %s%n", instruction.getAddress(), instruction);
                    instruction = instruction.getNext();
                }

                if (function != null) {
                    DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                    if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                        report.println("-- decompiler --");
                        report.println(result.getDecompiledFunction().getC());
                    } else {
                        report.println("Decompiler unavailable: " + result.getErrorMessage());
                    }
                }
                report.println();
            }
        } finally {
            decompiler.dispose();
        }

        println("Wrote Ghidra evidence report to " + output.getAbsolutePath());
    }
}
