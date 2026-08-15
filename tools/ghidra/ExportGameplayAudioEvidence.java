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

    private void dumpBytes(PrintWriter report, long start, int length, String label)
            throws Exception {
        byte[] bytes = new byte[length];
        currentProgram.getMemory().getBytes(toAddr(start), bytes);
        report.printf("==== %s $%04X-$%04X ====%n", label, start, start + length - 1);
        for (int offset = 0; offset < bytes.length; offset += 16) {
            report.printf("%04X:", start + offset);
            for (int index = 0; index < 16 && offset + index < bytes.length; ++index) {
                report.printf(" %02X", bytes[offset + index] & 0xFF);
            }
            report.println();
        }
        report.println();
    }

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
            report.println("Fixed-bank $AE8E requests $18 on a clean make; $AF2F/$AF34 request $1F and $22 when score state $06 underflows.");
            report.println("The request table resolves those score events to the bank-1 streams dumped below ($87B6/$87CA, $886D, and $8922).");
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
            dumpBytes(report, 0x8653L, 0x40, "live/dribble channel stream data");
            dumpBytes(report, 0x866BL, 0x20, "request $20 user-block noise stream");
            dumpBytes(report, 0x87A4L, 0x12, "request $10 CPU-block pulse/noise streams");
            dumpBytes(report, 0x87DDL, 0x18, "request $20 user-block pulse stream");
            dumpBytes(report, 0x87B6L, 0x30, "made-basket request $18 streams");
            dumpBytes(report, 0x886DL, 0x30, "post-score request $1F stream");
            dumpBytes(report, 0x8922L, 0x40, "post-score request $22 streams");
        } finally {
            decompiler.dispose();
        }
    }
}
