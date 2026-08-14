// Ghidra headless post-script for the bank-2 metasprite builder used by the intro.
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

public class ExportSpriteEvidence extends GhidraScript {
    private static final long[] ANCHORS = {
        0x8000L, 0x8020L, 0x8040L, 0x8070L, 0x80DAL, 0x80DFL,
        0x8109L, 0x810EL, 0x811AL, 0x8123L, 0x8150L, 0x81BCL
    };

    @Override
    public void run() throws Exception {
        File output = new File(getScriptArgs()[0]);
        output.getParentFile().mkdirs();
        PseudoDisassembler pseudo = new PseudoDisassembler(currentProgram);
        for (long value : ANCHORS) {
            Address address = toAddr(value);
            if (getInstructionAt(address) == null && pseudo.isValidSubroutine(address, true)) disassemble(address);
            if (getFunctionContaining(address) == null && getInstructionAt(address) != null) {
                createFunction(address, String.format("sprite_%04X", value));
            }
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter report = new PrintWriter(output, "UTF-8")) {
            report.println("Double Dribble Rev 1 - bank 2 intro metasprite evidence");
            for (long value : ANCHORS) {
                Address address = toAddr(value);
                Function function = getFunctionContaining(address);
                report.printf("==== $%04X ==== Function: %s%n", value,
                    function == null ? "<unresolved>" : function.getName());
                Instruction instruction = getInstructionAt(address);
                for (int count = 0; instruction != null && count < 64; count++) {
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
