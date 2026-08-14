// Ghidra headless post-script for the bank-0 tip-off setup and formation slice.
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

public class ExportTipoffEvidence extends GhidraScript {
    /* FCEUX execution counts and RAM writes from original frames 2320-2760 select these roots. */
    private static final long[] ANCHORS = {
        0x8491L, 0x89B2L, 0x8A16L, 0x8DFEL, 0x8E2BL, 0x8E39L,
        0x8E58L, 0x9102L, 0x914EL, 0x91A6L, 0x92BDL, 0x9308L,
        0x9311L, 0x9395L, 0x94A5L, 0x94D9L, 0x9583L, 0x994CL,
        0x9977L, 0x9ABDL, 0x9AF6L, 0x9B42L, 0x9B84L, 0x9BB0L,
        0x9BF6L, 0x9C06L, 0x9C0BL, 0x9CA0L, 0x9CF6L, 0x9D2DL,
        0x9E90L, 0x9EBDL, 0x9F26L, 0x9F70L, 0x9FA3L,
        0xA2C1L, 0xA603L, 0xA6C3L, 0xA84CL, 0xA85AL, 0xA896L, 0xAA07L, 0xAA20L,
        0xAA4AL, 0xAA9DL, 0xAAC4L, 0xAAEEL, 0xAB53L, 0xAB72L,
        0xAC83L, 0xACC7L, 0xAD0EL, 0xAD33L, 0xAE2CL, 0xAEDAL,
        0xB017L, 0xB035L, 0xB11FL, 0xB377L, 0xB3E9L, 0xB400L, 0xB473L,
        0xB501L, 0xB503L, 0xB58AL
    };

    @Override
    public void run() throws Exception {
        File output = new File(getScriptArgs()[0]);
        output.getParentFile().mkdirs();
        PseudoDisassembler pseudo = new PseudoDisassembler(currentProgram);
        for (long value : ANCHORS) {
            Address address = toAddr(value);
            if (getInstructionAt(address) == null && pseudo.isValidSubroutine(address, true)) {
                disassemble(address);
            }
            if (getFunctionContaining(address) == null && getInstructionAt(address) != null) {
                createFunction(address, String.format("tipoff_%04X", value));
            }
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter report = new PrintWriter(output, "UTF-8")) {
            report.println("Double Dribble Rev 1 - bank 0 tip-off evidence");
            for (long value : ANCHORS) {
                Address address = toAddr(value);
                Function function = getFunctionContaining(address);
                report.printf("==== $%04X ==== Function: %s%n", value,
                    function == null ? "<unresolved>" : function.getName());
                Instruction instruction = getInstructionAt(address);
                for (int count = 0; instruction != null && count < 96; count++) {
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
