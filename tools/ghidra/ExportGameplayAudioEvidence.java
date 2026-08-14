// Ghidra headless report for the bank-1 live gameplay audio driver and dribble streams.
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

public class ExportGameplayAudioEvidence extends GhidraScript {
    private static final long[] DRIVER_ANCHORS = {
        0x808AL, 0x80EDL, 0x81E9L, 0x821EL, 0x8241L,
        0x82C3L, 0x82D5L, 0x832BL, 0x83AAL, 0x847DL
    };

    @Override
    public void run() throws Exception {
        if (getScriptArgs().length != 1) throw new IllegalArgumentException("Expected output report path");
        File output = new File(getScriptArgs()[0]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();
        for (long value : DRIVER_ANCHORS) {
            Address address = toAddr(value);
            if (getInstructionAt(address) == null) disassemble(address);
            if (getFunctionContaining(address) == null && getInstructionAt(address) != null) {
                createFunction(address, String.format("gameplay_audio_%04X", value));
            }
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter report = new PrintWriter(output, "UTF-8")) {
            report.println("Double Dribble Rev 1 - bank 1 live gameplay audio evidence");
            report.println("FCEUX frame 2565 initializes channel streams $8653/$8664/$866B through fixed $CD24.");
            report.println("The same streams are requeued on the 18-frame dribble cadence; this is contextual gameplay audio, not continuous BGM.");
            report.println();
            for (long value : DRIVER_ANCHORS) {
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
            byte[] bytes = new byte[0x40];
            currentProgram.getMemory().getBytes(toAddr(0x8653L), bytes);
            report.println("==== channel stream data $8653-$8692 ====");
            for (int offset = 0; offset < bytes.length; offset += 16) {
                report.printf("%04X:", 0x8653 + offset);
                for (int index = 0; index < 16; ++index) report.printf(" %02X", bytes[offset + index] & 0xFF);
                report.println();
            }
        } finally {
            decompiler.dispose();
        }
    }
}
