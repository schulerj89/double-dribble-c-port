// Ghidra headless report for the bank-1 configuration music driver and score.
// @category DoubleDribble

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

public class ExportConfigMusicEvidence extends GhidraScript {
    private static final long[] DRIVER_ANCHORS = {
        0x808AL, 0x80EDL, 0x813DL, 0x8241L, 0x82C3L, 0x82D5L, 0x83AAL, 0x847DL
    };
    private static final long[][] DATA_RANGES = {
        {0x8598L, 0x60L},
        {0x89C5L, 0x120L}
    };

    @Override
    public void run() throws Exception {
        if (getScriptArgs().length != 1) throw new IllegalArgumentException("Expected output report path");
        File output = new File(getScriptArgs()[0]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();

        PseudoDisassembler pseudo = new PseudoDisassembler(currentProgram);
        for (long value : DRIVER_ANCHORS) {
            Address address = toAddr(value);
            if (getInstructionAt(address) == null && pseudo.isValidSubroutine(address, true)) disassemble(address);
            if (getFunctionContaining(address) == null && getInstructionAt(address) != null) {
                createFunction(address, String.format("config_music_%04X", value));
            }
        }

        try (PrintWriter report = new PrintWriter(output, "UTF-8")) {
            report.println("Double Dribble Rev 1 - bank 1 configuration music evidence");
            report.println("FCEUX channel pointers at frame 2092: pulse 1 $8A03, pulse 2 $89C5, triangle $8A4A");
            report.println("FCEUX first APU frame 2093; repeated channel state at frame 2989 (896-frame loop)");
            report.println();
            for (long value : DRIVER_ANCHORS) {
                Address address = toAddr(value);
                report.printf("==== driver $%04X ====%n", value);
                Function function = getFunctionContaining(address);
                report.println("Function: " + (function == null ? "<unresolved>" : function.getName()));
                for (Reference reference : getReferencesTo(address)) {
                    report.printf("Reference: %s (%s)%n", reference.getFromAddress(), reference.getReferenceType());
                }
                Instruction instruction = getInstructionAt(address);
                for (int count = 0; instruction != null && count < 48; count++) {
                    report.printf("%s  %s%n", instruction.getAddress(), instruction);
                    instruction = instruction.getNext();
                }
                report.println();
            }
            for (long[] range : DATA_RANGES) {
                report.printf("==== data $%04X-$%04X ====%n", range[0], range[0] + range[1] - 1L);
                byte[] bytes = new byte[(int)range[1]];
                currentProgram.getMemory().getBytes(toAddr(range[0]), bytes);
                for (int offset = 0; offset < bytes.length; offset += 16) {
                    report.printf("%04X:", range[0] + offset);
                    for (int index = 0; index < 16 && offset + index < bytes.length; index++) {
                        report.printf(" %02X", bytes[offset + index] & 0xFF);
                    }
                    report.println();
                }
                report.println();
            }
        }
        println("Wrote configuration music evidence to " + output.getAbsolutePath());
    }
}
