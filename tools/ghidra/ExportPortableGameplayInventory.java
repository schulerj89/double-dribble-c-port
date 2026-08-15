// Ghidra headless post-script: recursively inventory portable gameplay code.
// @category DoubleDribble

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.TreeMap;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class ExportPortableGameplayInventory extends GhidraScript {
    private static final long[] BANK0_ROOTS = {
        0x8195L, 0x8491L, 0x8594L, 0x85BEL, 0x860AL, 0x8682L,
        0x89B2L, 0x9635L, 0x9645L, 0x9651L, 0x98A3L, 0xA014L,
        0xA3E2L, 0xAC83L
    };
    private static final long[] FIXED_ROOTS = {
        0xC02BL, 0xC141L, 0xC477L, 0xC694L, 0xC6ADL,
        0xD368L, 0xD3C4L, 0xD3D5L, 0xD6BDL, 0xD6FDL, 0xD759L
    };

    private static class Routine {
        long address;
        int instructionCount;
        boolean truncated;
        Set<Long> calls = new LinkedHashSet<Long>();
        Set<Long> tails = new LinkedHashSet<Long>();
    }

    private boolean isMapped(long value) {
        Address address = toAddr(value);
        return currentProgram.getMemory().contains(address);
    }

    private long readWord(long address) throws Exception {
        int low = currentProgram.getMemory().getByte(toAddr(address)) & 0xff;
        int high = currentProgram.getMemory().getByte(toAddr(address + 1L)) & 0xff;
        return low | (high << 8);
    }

    private void addDispatchRoots(Set<Long> roots, long table, int count) throws Exception {
        for (int index = 0; index < count; ++index) {
            long target = readWord(table + index * 2L);
            if (isMapped(target)) roots.add(target);
        }
    }

    private void enqueueTarget(ArrayDeque<Long> routineQueue, Set<Long> known, long target) {
        if (isMapped(target) && known.add(target)) routineQueue.add(target);
    }

    private Routine scanRoutine(long start, ArrayDeque<Long> routineQueue,
                                Set<Long> knownRoutines) throws Exception {
        Routine routine = new Routine();
        routine.address = start;
        ArrayDeque<Long> blocks = new ArrayDeque<Long>();
        Set<Long> visited = new LinkedHashSet<Long>();
        blocks.add(start);
        while (!blocks.isEmpty() && visited.size() < 4096) {
            long value = blocks.removeFirst();
            while (isMapped(value) && visited.add(value)) {
                Address address = toAddr(value);
                Instruction instruction = getInstructionAt(address);
                if (instruction == null) {
                    disassemble(address);
                    instruction = getInstructionAt(address);
                }
                if (instruction == null) break;
                ++routine.instructionCount;
                String mnemonic = instruction.getMnemonicString().toUpperCase();
                Address[] flows = instruction.getFlows();
                Address fallThrough = instruction.getFallThrough();
                if (instruction.getFlowType().isCall() || "JSR".equals(mnemonic)) {
                    for (Address flow : flows) {
                        long target = flow.getOffset();
                        if (isMapped(target)) {
                            routine.calls.add(target);
                            enqueueTarget(routineQueue, knownRoutines, target);
                        }
                    }
                    if (fallThrough == null) break;
                    value = fallThrough.getOffset();
                    continue;
                }
                if (instruction.getFlowType().isJump() &&
                    !instruction.getFlowType().isConditional()) {
                    for (Address flow : flows) {
                        long target = flow.getOffset();
                        if (isMapped(target)) {
                            routine.tails.add(target);
                            enqueueTarget(routineQueue, knownRoutines, target);
                        }
                    }
                    break;
                }
                if (instruction.getFlowType().isConditional()) {
                    for (Address flow : flows) {
                        if (isMapped(flow.getOffset())) blocks.add(flow.getOffset());
                    }
                    if (fallThrough == null) break;
                    value = fallThrough.getOffset();
                    continue;
                }
                if (instruction.getFlowType().isTerminal() ||
                    "RTS".equals(mnemonic) || "RTI".equals(mnemonic) ||
                    fallThrough == null) break;
                value = fallThrough.getOffset();
            }
        }
        routine.truncated = visited.size() >= 4096;
        return routine;
    }

    private String addressString(long value) {
        return String.format("%04X", value & 0xffffL);
    }

    private void writeAddressArray(PrintWriter report, Set<Long> values) {
        List<Long> sorted = new ArrayList<Long>(values);
        Collections.sort(sorted);
        report.print("[");
        for (int index = 0; index < sorted.size(); ++index) {
            if (index != 0) report.print(", ");
            report.printf("\"%s\"", addressString(sorted.get(index)));
        }
        report.print("]");
    }

    @Override
    public void run() throws Exception {
        if (getScriptArgs().length != 2) {
            throw new IllegalArgumentException("Expected bank kind and output JSON path");
        }
        String bank = getScriptArgs()[0];
        Set<Long> roots = new LinkedHashSet<Long>();
        long[] configuredRoots = "fixed7".equals(bank) ? FIXED_ROOTS : BANK0_ROOTS;
        for (long root : configuredRoots) if (isMapped(root)) roots.add(root);
        if ("bank0".equals(bank)) {
            addDispatchRoots(roots, 0x89C0L, 34);
            addDispatchRoots(roots, 0xA01CL, 18);
            addDispatchRoots(roots, 0xAC91L, 13);
        }

        ArrayDeque<Long> queue = new ArrayDeque<Long>();
        Set<Long> known = new LinkedHashSet<Long>();
        TreeMap<Long, Routine> routines = new TreeMap<Long, Routine>();
        for (long root : roots) enqueueTarget(queue, known, root);
        while (!queue.isEmpty()) {
            long address = queue.removeFirst();
            if (!routines.containsKey(address)) {
                routines.put(address, scanRoutine(address, queue, known));
            }
        }

        File output = new File(getScriptArgs()[1]);
        if (output.getParentFile() != null) output.getParentFile().mkdirs();
        try (PrintWriter report = new PrintWriter(output, "UTF-8")) {
            report.println("{");
            report.println("  \"schema\": 1,");
            report.printf("  \"bank\": \"%s\",%n", bank);
            report.print("  \"roots\": ");
            writeAddressArray(report, roots);
            report.println(",");
            report.println("  \"routines\": [");
            int emitted = 0;
            for (Routine routine : routines.values()) {
                if (emitted++ != 0) report.println(",");
                report.println("    {");
                report.printf("      \"address\": \"%s\",%n", addressString(routine.address));
                report.printf("      \"instruction_count\": %d,%n", routine.instructionCount);
                report.printf("      \"truncated\": %s,%n", routine.truncated ? "true" : "false");
                report.print("      \"calls\": ");
                writeAddressArray(report, routine.calls);
                report.println(",");
                report.print("      \"tail_calls\": ");
                writeAddressArray(report, routine.tails);
                report.println(",");
                report.println("      \"classification\": \"unclassified\",");
                report.println("      \"status\": \"inventory_pending\"");
                report.print("    }");
            }
            report.println();
            report.println("  ]");
            report.println("}");
        }
        println(String.format("Portable gameplay inventory %s: %d roots, %d routines",
            bank, roots.size(), routines.size()));
    }
}
