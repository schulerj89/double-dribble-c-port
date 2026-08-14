-- Focused trace from the pre-jump formation through the first live possession.
-- Outputs are reverse-engineering evidence under ignored captures/ directories.

local capture_root = os.getenv("DD_CAPTURE_ROOT") or "."
local rom_path = os.getenv("DD_ROM_PATH") or ""
local final_frame = tonumber(os.getenv("DD_CAPTURE_FINAL_FRAME") or "2760")
local trace_start = tonumber(os.getenv("DD_TRACE_START") or "2320")
local trace_end = tonumber(os.getenv("DD_TRACE_END") or "2760")
local jump_start = tonumber(os.getenv("DD_TIP_JUMP_START") or "-1")
local jump_end = tonumber(os.getenv("DD_TIP_JUMP_END") or "-1")
local jump_button = os.getenv("DD_TIP_JUMP_BUTTON") or "A"

local function join_path(left, right)
    local suffix = string.sub(left, -1)
    if suffix == "\\" or suffix == "/" then return left .. right end
    return left .. "\\" .. right
end

local function read_file(path)
    local file = assert(io.open(path, "rb"))
    local data = file:read("*all")
    file:close()
    return data
end

local function write_file(path, data)
    local file = assert(io.open(path, "wb"))
    file:write(data)
    file:close()
end

local rom_data = read_file(rom_path)
local function current_switch_bank()
    for bank = 0, 7 do
        local matched = true
        local bank_start = 16 + bank * 0x4000
        for index = 0, 15 do
            if memory.readbyte(0x8000 + index) ~= string.byte(rom_data, bank_start + index + 1) then
                matched = false
                break
            end
        end
        if matched then return bank end
    end
    return -1
end

local function phase_for_frame(frame)
    if frame < 2400 then return "formation" end
    if frame < 2531 then return "toss_jump" end
    if frame < 2557 then return "possession_award" end
    return "live"
end

local writes = assert(io.open(join_path(capture_root, "tipoff-writes.csv"), "w"))
writes:write("frame,phase,address,value,pc,bank\n")
local ppu_writes = assert(io.open(join_path(capture_root, "tipoff-ppu-writes.csv"), "w"))
ppu_writes:write("frame,register,value,pc,bank\n")
local apu_writes = assert(io.open(join_path(capture_root, "gameplay-apu-writes.csv"), "w"))
apu_writes:write("frame,address,value,pc,bank\n")
local audio_state = assert(io.open(join_path(capture_root, "gameplay-audio-state.csv"), "w"))
audio_state:write("frame,square1_period,square1_volume,square1_duty,square2_period,square2_volume,square2_duty,triangle_period,triangle_volume,noise_period,noise_volume,noise_short\n")
local audio_ram = assert(io.open(join_path(capture_root, "gameplay-audio-ram-writes.csv"), "w"))
audio_ram:write("frame,address,value,pc,bank\n")
local states = assert(io.open(join_path(capture_root, "tipoff-state.bin"), "wb"))
local counts = {formation = {}, toss_jump = {}, possession_award = {}, live = {}}

local function record_write(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end then
        writes:write(string.format("%d,%s,%04X,%02X,%04X,%d\n",
            frame, phase_for_frame(frame), address, value,
            memory.getregister("pc"), current_switch_bank()))
    end
end

memory.registerwrite(0x0030, 0x40, record_write)
memory.registerwrite(0x0340, 0x380, record_write)
memory.registerwrite(0x0700, 0xE0, record_write)
memory.registerwrite(0x07E0, 0x20, record_write)
memory.registerwrite(0x2000, 1, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end then
        ppu_writes:write(string.format("%d,%04X,%02X,%04X,%d\n",
            frame, address, value, memory.getregister("pc"), current_switch_bank()))
    end
end)
memory.registerwrite(0x2005, 3, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end then
        ppu_writes:write(string.format("%d,%04X,%02X,%04X,%d\n",
            frame, address, value, memory.getregister("pc"), current_switch_bank()))
    end
end)
memory.registerwrite(0x4000, 0x18, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end then
        apu_writes:write(string.format("%d,%04X,%02X,%04X,%d\n",
            frame, address, value, memory.getregister("pc"), current_switch_bank()))
    end
end)
memory.registerwrite(0x0079, 0x60, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end then
        audio_ram:write(string.format("%d,%04X,%02X,%04X,%d\n",
            frame, address, value, memory.getregister("pc"), current_switch_bank()))
    end
end)
memory.registerexecute(0x8000, 0x4000, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
        local phase = phase_for_frame(frame)
        counts[phase][address] = (counts[phase][address] or 0) + 1
    end
end)

local capture_frames = {
    [2320]=true,[2340]=true,[2360]=true,[2380]=true,[2400]=true,[2420]=true,[2440]=true,
    [2460]=true,[2480]=true,[2500]=true,[2519]=true,[2520]=true,[2530]=true,[2531]=true,
    [2540]=true,[2550]=true,[2557]=true,[2560]=true,[2570]=true,[2580]=true,[2590]=true,
    [2600]=true,[2620]=true,[2640]=true,
    [2660]=true,[2680]=true,[2700]=true,[2723]=true,[2749]=true,[2760]=true,
    [2770]=true,[2783]=true,[2929]=true,[2944]=true,[3004]=true,[3324]=true,
    [3501]=true,[3545]=true,[3553]=true,[3572]=true,[3600]=true,[3640]=true,
    [3680]=true,[3720]=true,[3800]=true,[3900]=true,[4000]=true,[4100]=true,[4200]=true
}

local function write_state(frame)
    states:write(string.char(frame % 256, math.floor(frame / 256) % 256))
    local bytes = {}
    for address = 0, 0x07FF do bytes[#bytes + 1] = string.char(memory.readbyte(address)) end
    states:write(table.concat(bytes))
end

emu.poweron()
while emu.framecount() < final_frame do
    local next_frame = emu.framecount() + 1
    local input = {}
    input.start = next_frame == 75 or next_frame == 76
    input.down = next_frame == 2105 or next_frame == 2107 or next_frame == 2109
    input.A = next_frame == 2112 or next_frame == 2113
    if next_frame >= jump_start and next_frame <= jump_end then
        input[jump_button] = true
    end
    joypad.set(1, input)
    emu.frameadvance()
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end then
        local snd = sound.get().rp2a03
        write_state(frame)
        audio_state:write(string.format("%d,%d,%.6f,%d,%d,%.6f,%d,%d,%.6f,%d,%.6f,%s\n",
            frame,
            snd.square1.regs.frequency, snd.square1.volume, snd.square1.duty,
            snd.square2.regs.frequency, snd.square2.volume, snd.square2.duty,
            snd.triangle.regs.frequency, snd.triangle.volume,
            snd.noise.regs.frequency, snd.noise.volume, tostring(snd.noise.short)))
    end
    if capture_frames[frame] then
        gui.savescreenshotas(join_path(capture_root, string.format("frame-%04d.png", frame)))
        write_file(join_path(capture_root, string.format("frame-%04d-ppu.bin", frame)),
                   ppu.readbyterange(0, 0x4000))
    end
end

writes:close()
ppu_writes:close()
apu_writes:close()
audio_state:close()
audio_ram:close()
states:close()
local count_file = assert(io.open(join_path(capture_root, "tipoff-pc-counts.csv"), "w"))
count_file:write("phase,address,count\n")
for phase, phase_counts in pairs(counts) do
    for address, count in pairs(phase_counts) do
        count_file:write(string.format("%s,%04X,%d\n", phase, address, count))
    end
end
count_file:close()
emu.exit()
