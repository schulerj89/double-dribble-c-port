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
        0x8195L, 0x81A2L, 0x8266L, 0x8297L, 0x829EL, 0x8371L, 0x83C5L, 0x8460L,
        0x89B2L, 0x8A16L, 0x8A3AL, 0x8A98L, 0x8AF4L, 0x8B12L, 0x8B5AL, 0x8BC5L,
        0x8C6BL, 0x8D1FL, 0x8D57L, 0x8D9CL, 0x8DABL, 0x8DD2L, 0x8DF7L,
        0x8E71L, 0x8E88L, 0x8EBFL, 0x8EE2L, 0x8FE0L, 0x904DL, 0x9094L,
        0x9102L, 0x91A6L, 0x92BDL, 0x9395L, 0x9431L, 0x9490L, 0x9583L, 0x9645L,
        0xA347L, 0xA61FL, 0xA68AL, 0xA6C3L,
        0x993AL, 0x9B42L, 0x9E70L, 0xAC83L, 0xACABL, 0xACB6L, 0xACD6L,
        0xAD41L, 0xADF2L, 0xAE0CL, 0xAE25L, 0xAEDEL,
        0xAF46L, 0xAF72L, 0xAFDDL, 0xB017L, 0xB138L, 0xB167L, 0xB377L, 0xB435L,
        0xB473L, 0xB51DL, 0x9ABDL
    };
    private static final long[] FIXED_ANCHORS = {
        0xC41CL, 0xC694L, 0xCC94L, 0xCD24L, 0xCD5CL, 0xCD64L,
        0xCEF3L, 0xCF33L, 0xCF37L, 0xCF88L, 0xCF8CL,
        0xCFD5L, 0xD01FL, 0xD069L, 0xD0AEL, 0xD0FAL,
        0xD148L, 0xD18CL, 0xD1D0L, 0xD214L, 0xD258L,
        0xD368L, 0xD3C4L, 0xD3D5L, 0xD6FDL, 0xD759L, 0xD772L,
        0xD8DAL, 0xD978L, 0xD99AL, 0xDB0EL
    };

    private void printDispatchTable(PrintWriter report, String name, long tableAddress,
                                    int firstState, int stateCount) throws Exception {
        report.printf("==== %s dispatcher table at $%04X ====%n", name, tableAddress);
        for (int index = 0; index < stateCount; ++index) {
            int low = currentProgram.getMemory().getByte(toAddr(tableAddress + index * 2L)) & 0xff;
            int high = currentProgram.getMemory().getByte(toAddr(tableAddress + index * 2L + 1L)) & 0xff;
            report.printf("state $%02X -> $%04X%n", firstState + index, low | (high << 8));
        }
        report.println();
    }

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
            if (!"fixed".equals(getScriptArgs()[0])) {
                printDispatchTable(report, "player", 0x89C0L, 0x20, 34);
                printDispatchTable(report, "ball", 0xAC91L, 0x00, 13);
            }
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
