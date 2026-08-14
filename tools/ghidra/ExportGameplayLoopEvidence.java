// Ghidra headless post-script for the live player/ball/inbound call graph.
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

public class ExportGameplayLoopEvidence extends GhidraScript {
    private static final long[] BANK0_ANCHORS = {
        0x89B2L, 0x8B5AL, 0x8D1FL, 0x8D57L, 0x8D9CL,
        0x91A6L, 0x92BDL, 0x9395L, 0x9583L, 0x9645L,
        0xA61FL, 0xA68AL,
        0x993AL, 0x9E70L, 0xAC83L, 0xACB6L, 0xACD6L,
        0xAD41L, 0xADF2L, 0xAE0CL, 0xAE25L, 0xAEDEL,
        0xAF46L, 0xAF72L, 0xAFDDL, 0xB017L, 0xB377L,
        0xB473L, 0xB51DL
    };
    private static final long[] FIXED_ANCHORS = {
        0xC41CL, 0xD6FDL, 0xD759L, 0xD772L, 0xD8DAL,
        0xD978L, 0xD99AL, 0xDB0EL
    };

    @Override
    public void run() throws Exception {
        if (getScriptArgs().length != 2) {
            throw new IllegalArgumentException("Expected bank kind and output report path");
        }
        long[] anchors = "fixed".equals(getScriptArgs()[0]) ? FIXED_ANCHORS : BANK0_ANCHORS;
        File output = new File(getScriptArgs()[1]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();
        PseudoDisassembler pseudo = new PseudoDisassembler(currentProgram);
        for (long value : anchors) {
            Address address = toAddr(value);
            if (getInstructionAt(address) == null) disassemble(address);
            if (getFunctionContaining(address) == null && getInstructionAt(address) != null) {
                createFunction(address, String.format("gameplay_%04X", value));
            }
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter report = new PrintWriter(output, "UTF-8")) {
            report.printf("Double Dribble Rev 1 - %s gameplay call graph evidence%n", getScriptArgs()[0]);
            for (long value : anchors) {
                Address address = toAddr(value);
                Function function = getFunctionContaining(address);
                report.printf("==== $%04X ==== Function: %s%n", value,
                    function == null ? "<unresolved>" : function.getName());
                report.println("Call/reference sites:");
                Reference[] references = getReferencesTo(address);
                if (references.length == 0) report.println("  <none>");
                for (Reference reference : references) {
                    report.printf("  %s (%s)%n", reference.getFromAddress(), reference.getReferenceType());
                }
                Instruction instruction = getInstructionAt(address);
                for (int count = 0; instruction != null && count < 128; ++count) {
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
            }
        } finally {
            decompiler.dispose();
        }
    }
}
