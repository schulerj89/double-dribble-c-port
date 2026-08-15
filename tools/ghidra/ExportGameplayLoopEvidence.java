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
        0x8491L, 0x84EEL,
        0x8594L, 0x85BEL, 0x860AL, 0x8682L, 0x86A7L, 0x877FL, 0x886CL, 0x88D4L,
        0x8919L, 0x897FL,
        0x89B2L, 0x8A16L, 0x8A3AL, 0x8A98L, 0x8AF4L, 0x8B12L, 0x8B5AL, 0x8BC5L, 0x8BF8L,
        0x8C36L, 0x8C6BL, 0x8CF3L, 0x8D1FL, 0x8D57L, 0x8D9CL, 0x8DABL, 0x8DD2L, 0x8DF7L,
        0x8E71L, 0x8E88L, 0x8EBFL, 0x8EE2L, 0x8F0BL, 0x8F4CL, 0x8FB9L,
        0x8FE0L, 0x9018L, 0x904DL, 0x9094L,
        0x9102L, 0x914EL, 0x91A6L, 0x9208L, 0x92BDL, 0x9395L, 0x93AEL, 0x9400L, 0x9413L, 0x9418L,
        0x9431L, 0x9490L, 0x9583L, 0x95D0L, 0x9635L, 0x9645L, 0x9651L, 0x9698L, 0x96B4L, 0x9707L,
        0x98A3L, 0x98B5L, 0xA014L, 0xA0B2L, 0xA0C3L, 0xA0D9L, 0xA0DAL, 0xA129L, 0xA1CCL,
        0xA21FL, 0xA29DL, 0xA347L, 0xA37DL, 0xA3E2L, 0xA44BL, 0xA482L, 0xA504L, 0xA556L, 0xA557L, 0xA5D0L,
        0x9B84L, 0x9BB0L, 0x9CA0L, 0x9CF6L, 0x9D2DL, 0x9E2DL, 0x9E90L,
        0xA607L, 0xA61FL, 0xA638L, 0xA68AL, 0xA6C3L, 0xA712L, 0xA765L, 0xA780L, 0xA7EAL, 0xA84CL,
        0xA85AL, 0xA896L, 0x993AL, 0x99D9L, 0x9A31L, 0x9B42L, 0x9E70L, 0x9FA3L,
        0xAA20L, 0xAA75L, 0xAAEEL, 0xAB96L, 0xABCDL,
        0xAC2AL, 0xAC5CL, 0xAC64L, 0xAC83L, 0xACABL, 0xACB6L, 0xACD6L,
        0xAD41L, 0xAD58L, 0xAD6DL, 0xADF2L, 0xAE0CL, 0xAE25L, 0xAEDEL,
        0xAF46L, 0xAF72L, 0xAFDDL, 0xB017L, 0xB0ABL, 0xB138L, 0xB167L, 0xB189L,
        0xB29CL, 0xB2EEL, 0xB2F8L, 0xB305L, 0xB318L, 0xB32BL, 0xB377L, 0xB3E9L,
        0xB400L, 0xB435L, 0xB501L, 0xB503L,
        0xB473L, 0xB51DL, 0x9ABDL
    };
    private static final long[] FIXED_ANCHORS = {
        0xC02BL, 0xC02FL, 0xC141L, 0xC3C5L, 0xC41CL, 0xC477L, 0xC694L, 0xC6ADL, 0xCC94L, 0xCD24L, 0xCD5CL, 0xCD64L,
        0xCEF3L, 0xCF33L, 0xCF37L, 0xCF88L, 0xCF8CL,
        0xCFD5L, 0xD01FL, 0xD069L, 0xD0AEL, 0xD0FAL,
        0xD148L, 0xD18CL, 0xD1D0L, 0xD214L, 0xD258L,
        0xD368L, 0xD3C4L, 0xD3D5L, 0xD6BDL, 0xD6FDL, 0xD759L, 0xD772L,
        0xD7CCL, 0xD7DEL, 0xD820L, 0xD834L, 0xD857L, 0xD862L, 0xD885L,
        0xD8AAL, 0xD8B0L, 0xD8F1L, 0xD8FAL, 0xD978L, 0xD98AL, 0xD98DL,
        0xD99AL, 0xDB0EL
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

    private void printByteTable(PrintWriter report, String name, long tableAddress,
                                int count) throws Exception {
        report.printf("==== %s byte table at $%04X ====%n", name, tableAddress);
        for (int index = 0; index < count; ++index) {
            if ((index & 15) == 0) report.printf("$%04X:", tableAddress + index);
            report.printf(" %02X", currentProgram.getMemory().getByte(
                toAddr(tableAddress + index)) & 0xff);
            if ((index & 15) == 15 || index + 1 == count) report.println();
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
                printDispatchTable(report, "user player", 0xA01CL, 0x00, 18);
                printDispatchTable(report, "ball", 0xAC91L, 0x00, 13);
                printByteTable(report, "CPU avoidance direction", 0x8BC8L, 32);
                printByteTable(report, "made-basket target phase", 0x8503L, 4);
                printByteTable(report, "made-basket target/action", 0x8507L, 40);
                printByteTable(report, "movement depth vectors", 0x9C1CL, 66);
                printByteTable(report, "movement longitudinal vectors", 0x9C5EL, 66);
                printByteTable(report, "movement angle thresholds", 0x9DEBL, 66);
                printByteTable(report, "eight-way input vectors", 0x9E4CL, 32);
                printByteTable(report, "inbound packed adjustment", 0x9763L, 256);
                printByteTable(report, "exceptional contact opposite facing", 0xA375L, 8);
                printByteTable(report, "three-point depth boundary", 0xA834L, 23);
                printByteTable(report, "player animation pointers", 0xA8E6L, 68);
                printByteTable(report, "shooting metasprites by facing", 0xA9DCL, 8);
                printByteTable(report, "user shooting height-script pointer", 0x9B26L, 2);
            } else {
                printByteTable(report, "CPU region policy offsets", 0xD8B9L, 28);
                printByteTable(report, "CPU packed target policy", 0xD8D5L, 28);
                printByteTable(report, "CPU receiver region eligibility", 0xD94EL, 42);
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
