// Ghidra headless post-script for the bank-1 road-intro and music-driver slice.
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

public class ExportIntroEvidence extends GhidraScript {
    private static final long[] ANCHORS = {
        0x8000L, 0x8079L, 0x808AL, 0x80EDL, 0x813DL, 0x81D0L,
        0x8241L, 0x82C3L, 0x82D5L, 0x83AAL, 0x847DL, 0x8493L,
        0x9329L, 0x9348L, 0x9357L, 0x937FL, 0x938DL, 0x9396L, 0x939DL,
        0x93A3L, 0x93ABL, 0x93B8L, 0x942FL, 0x944CL, 0x9472L,
        0x9495L, 0x94A5L, 0x94B2L, 0x94BCL, 0x94C6L, 0x94D2L, 0x94DEL,
        0xA25BL, 0xA263L, 0xA2A1L, 0xA2CCL, 0xA307L, 0xA32DL
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
            if (getInstructionAt(address) == null) {
                if (pseudo.isValidSubroutine(address, true) || value >= 0xA25BL) {
                    disassemble(address);
                }
            }
            if (getFunctionContaining(address) == null && getInstructionAt(address) != null) {
                createFunction(address, String.format("intro_%04X", value));
            }
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter report = new PrintWriter(output, "UTF-8")) {
            report.println("Double Dribble Rev 1 - bank 1 intro/music evidence");
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
                for (int count = 0; instruction != null && count < 48; count++) {
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
        println("Wrote bank-1 intro evidence report to " + output.getAbsolutePath());
    }
}
