-- Focused FCEUX trace for the game-configuration music.
--
-- This records the live APU channel state and the bank-1 driver PCs without
-- producing a playback log used by the native runtime. Outputs stay under the
-- ignored captures/ directory.

local capture_root = os.getenv("DD_CAPTURE_ROOT") or "."
local rom_path = os.getenv("DD_ROM_PATH") or ""
local start_frame = tonumber(os.getenv("DD_START_FRAME") or "75")
local final_frame = tonumber(os.getenv("DD_CAPTURE_FINAL_FRAME") or "4200")
local trace_start = tonumber(os.getenv("DD_TRACE_START") or "2080")

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

local rom_data = rom_path ~= "" and read_file(rom_path) or nil

local function current_switch_bank()
    if rom_data == nil or string.len(rom_data) < 16 + 8 * 0x4000 then return -1 end
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

local state_file = assert(io.open(join_path(capture_root, "config-music-state.csv"), "w"))
state_file:write("frame,square1_period,square1_volume,square1_duty,square2_period,square2_volume,square2_duty,triangle_period,triangle_volume,noise_period,noise_volume,noise_short\n")

local write_file = assert(io.open(join_path(capture_root, "config-music-writes.csv"), "w"))
write_file:write("frame,address,value,pc,bank\n")

local ram_file = assert(io.open(join_path(capture_root, "config-music-ram-writes.csv"), "w"))
ram_file:write("frame,address,value,pc,bank\n")

local pc_counts = {}

memory.registerwrite(0x4000, 0x10, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start then
        write_file:write(string.format("%d,%04X,%02X,%04X,%d\n",
            frame, address, value, memory.getregister("pc"), current_switch_bank()))
    end
end)

memory.registerwrite(0x0079, 0x60, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start then
        ram_file:write(string.format("%d,%04X,%02X,%04X,%d\n",
            frame, address, value, memory.getregister("pc"), current_switch_bank()))
    end
end)

memory.registerexecute(0x8000, 0x4000, function(address, size, value)
    if emu.framecount() >= trace_start and current_switch_bank() == 1 then
        pc_counts[address] = (pc_counts[address] or 0) + 1
    end
end)

emu.poweron()
while emu.framecount() < final_frame do
    local next_frame = emu.framecount() + 1
    joypad.set(1, {start = next_frame == start_frame or next_frame == start_frame + 1})
    emu.frameadvance()
    local frame = emu.framecount()
    if frame == 2097 then
        gui.savescreenshotas(join_path(capture_root, "config-music-frame-2097.png"))
    end
    if frame >= trace_start then
        local snd = sound.get().rp2a03
        state_file:write(string.format("%d,%d,%.6f,%d,%d,%.6f,%d,%d,%.6f,%d,%.6f,%s\n",
            frame,
            snd.square1.regs.frequency, snd.square1.volume, snd.square1.duty,
            snd.square2.regs.frequency, snd.square2.volume, snd.square2.duty,
            snd.triangle.regs.frequency, snd.triangle.volume,
            snd.noise.regs.frequency, snd.noise.volume, tostring(snd.noise.short)))
    end
end

state_file:close()
write_file:close()
ram_file:close()

local count_file = assert(io.open(join_path(capture_root, "config-music-pc-counts.csv"), "w"))
count_file:write("address,count\n")
for address, count in pairs(pc_counts) do
    count_file:write(string.format("%04X,%d\n", address, count))
end
count_file:close()
emu.exit()
