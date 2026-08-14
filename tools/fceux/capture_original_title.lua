-- Reproducible FCEUX reference capture for the Double Dribble title milestone.
--
-- Outputs are reverse-engineering evidence and must remain under the ignored
-- captures/ directory. Set DD_CAPTURE_ROOT and DD_ROM_PATH before launching.

local capture_root = os.getenv("DD_CAPTURE_ROOT") or "."
local rom_path = os.getenv("DD_ROM_PATH") or ""
local final_frame = tonumber(os.getenv("DD_CAPTURE_FINAL_FRAME") or "600")
local start_frame = tonumber(os.getenv("DD_START_FRAME") or "-1")
local intro_trace = os.getenv("DD_INTRO_TRACE") == "1"
local trace_start = tonumber(os.getenv("DD_TRACE_START") or "166")
local trace_end = tonumber(os.getenv("DD_TRACE_END") or "240")

local capture_frames = {
    [30] = true,
    [60] = true,
    [75] = true,
    [76] = true,
    [77] = true,
    [80] = true,
    [90] = true,
    [120] = true,
    [150] = true,
    [158] = true,
    [161] = true,
    [164] = true,
    [165] = true,
    [166] = true,
    [170] = true,
    [180] = true,
    [210] = true,
    [240] = true,
    [300] = true,
    [360] = true,
    [420] = true,
    [480] = true,
    [540] = true,
    [600] = true,
    [660] = true,
    [720] = true,
    [780] = true,
    [840] = true,
    [900] = true,
    [960] = true,
    [1020] = true,
    [1080] = true,
    [1140] = true,
    [1200] = true,
    [1260] = true,
    [1320] = true,
    [1380] = true,
    [1440] = true,
    [1500] = true,
}

local function join_path(left, right)
    local suffix = string.sub(left, -1)
    if suffix == "\\" or suffix == "/" then
        return left .. right
    end
    return left .. "\\" .. right
end

local function read_file(path)
    local file = assert(io.open(path, "rb"))
    local data = file:read("*all")
    file:close()
    return data
end

local rom_data = nil
if rom_path ~= "" then
    rom_data = read_file(rom_path)
end

local function current_switch_bank()
    if rom_data == nil or string.len(rom_data) < 16 + (8 * 0x4000) then
        return -1
    end

    for bank = 0, 7 do
        local matched = true
        local bank_start = 16 + (bank * 0x4000)
        for index = 0, 15 do
            local cpu_value = memory.readbyte(0x8000 + index)
            local rom_value = string.byte(rom_data, bank_start + index + 1)
            if cpu_value ~= rom_value then
                matched = false
                break
            end
        end
        if matched then
            return bank
        end
    end
    return -1
end

local log_path = join_path(capture_root, "trace.csv")
local log_file = assert(io.open(log_path, "w"))
log_file:write("frame,event,address,value,pc,bank,a,x,y,p,zp00,zp01,zp02\n")
local intro_pc_counts = {}

local function log_event(event_name, address, value)
    log_file:write(string.format(
        "%d,%s,%04X,%02X,%04X,%d,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
        emu.framecount(),
        event_name,
        address,
        value,
        memory.getregister("pc"),
        current_switch_bank(),
        memory.getregister("a"),
        memory.getregister("x"),
        memory.getregister("y"),
        memory.getregister("p"),
        memory.readbyte(0x0000),
        memory.readbyte(0x0001),
        memory.readbyte(0x0002)
    ))
    log_file:flush()
end

local function on_apu_write(address, size, value)
    log_event("apu-write", address, value)
end

local function on_mapper_write(address, size, value)
    log_event("mapper-write", address, value)
end

local function on_ppu_register_write(address, size, value)
    -- Register only the control/address/data path used by screen loaders. This
    -- remains diagnostic evidence; no PPU write log is a runtime dependency.
    log_event("ppu-write", address, value)
end

memory.registerwrite(0x4000, 0x16, on_apu_write)
memory.registerwrite(0x8000, 0x8000, on_mapper_write)
memory.registerwrite(0x2000, 0x08, on_ppu_register_write)
memory.registerexecute(0xC566, function(address, size, value)
    log_event("stream-entry", address, value)
end)
memory.registerexecute(0xC5C3, function(address, size, value)
    log_event("stream-exit", address, value)
end)
memory.registerexecute(0xCD70, function(address, size, value)
    log_event("dmc-entry", address, value)
end)

if intro_trace then
    memory.registerwrite(0x0200, 0x0600, function(address, size, value)
        local frame = emu.framecount()
        if frame >= trace_start and frame <= trace_end then
            log_event("intro-ram-write", address, value)
        end
    end)
    memory.registerexecute(0x8000, 0x4000, function(address, size, value)
        local frame = emu.framecount()
        if frame >= trace_start and frame <= trace_end and current_switch_bank() == 1 then
            intro_pc_counts[address] = (intro_pc_counts[address] or 0) + 1
        end
    end)
end

local function write_binary(path, bytes)
    local file = assert(io.open(path, "wb"))
    file:write(bytes)
    file:close()
end

local function capture(frame)
    local stem = string.format("frame-%04d", frame)
    gui.savescreenshotas(join_path(capture_root, stem .. ".png"))
    write_binary(join_path(capture_root, stem .. "-ppu.bin"), ppu.readbyterange(0, 0x4000))

    local ram = {}
    for address = 0, 0x07FF do
        ram[#ram + 1] = string.char(memory.readbyte(address))
    end
    write_binary(join_path(capture_root, stem .. "-ram.bin"), table.concat(ram))

    log_event("capture", 0, 0)
end

emu.poweron()
while emu.framecount() < final_frame do
    local next_frame = emu.framecount() + 1
    joypad.set(1, { start = next_frame == start_frame or next_frame == start_frame + 1 })
    emu.frameadvance()
    local frame = emu.framecount()
    if capture_frames[frame] or (frame > 1500 and frame % 60 == 0) then
        capture(frame)
    end
end

log_file:close()
if intro_trace then
    local count_file = assert(io.open(join_path(capture_root, "intro-pc-counts.csv"), "w"))
    count_file:write("address,count\n")
    for address, count in pairs(intro_pc_counts) do
        count_file:write(string.format("%04X,%d\n", address, count))
    end
    count_file:close()
end
emu.exit()
