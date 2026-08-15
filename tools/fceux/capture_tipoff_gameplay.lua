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
local pass_frame = tonumber(os.getenv("DD_PASS_FRAME") or "-1")
local pass_end = tonumber(os.getenv("DD_PASS_END") or tostring(pass_frame))
local pass_button = os.getenv("DD_PASS_BUTTON") or "A"
local pass_direction = os.getenv("DD_PASS_DIRECTION") or "none"
local enable_pc_counts = os.getenv("DD_ENABLE_PC_COUNTS") ~= "0"
local inject_rim_frame = tonumber(os.getenv("DD_INJECT_RIM_FRAME") or "-1")
local inject_contact_frame = tonumber(os.getenv("DD_INJECT_CONTACT_FRAME") or "-1")
local inject_contact_clock_gate = tonumber(os.getenv("DD_INJECT_CONTACT_CLOCK_GATE") or "-1")
local inject_basket_frame = tonumber(os.getenv("DD_INJECT_BASKET_FRAME") or "-1")
local inject_basket_result = tonumber(os.getenv("DD_INJECT_BASKET_RESULT") or "1")
local inject_basket_counter = tonumber(os.getenv("DD_INJECT_BASKET_COUNTER") or "0")
local inject_ball_state_frame = tonumber(os.getenv("DD_INJECT_BALL_STATE_FRAME") or "-1")
local inject_ball_state = tonumber(os.getenv("DD_INJECT_BALL_STATE") or "12")
local inject_loose_launch_frame = tonumber(os.getenv("DD_INJECT_LOOSE_LAUNCH_FRAME") or "-1")
local inject_loose_outcome = tonumber(os.getenv("DD_INJECT_LOOSE_OUTCOME") or "2")
local inject_player_state_frame = tonumber(os.getenv("DD_INJECT_PLAYER_STATE_FRAME") or "-1")
local inject_player_state = tonumber(os.getenv("DD_INJECT_PLAYER_STATE") or "59")
local inject_player_slot = tonumber(os.getenv("DD_INJECT_PLAYER_SLOT") or "7")
local inject_player_route_case = tonumber(os.getenv("DD_INJECT_PLAYER_ROUTE_CASE") or "0")
local inject_player_jump_case = tonumber(os.getenv("DD_INJECT_PLAYER_JUMP_CASE") or "0")
local inject_player_inbound_case = tonumber(os.getenv("DD_INJECT_PLAYER_INBOUND_CASE") or "0")
local inject_player_pair_case = tonumber(os.getenv("DD_INJECT_PLAYER_PAIR_CASE") or "0")
local inject_player_hold_case = tonumber(os.getenv("DD_INJECT_PLAYER_HOLD_CASE") or "0")

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
local dispatch = assert(io.open(join_path(capture_root, "gameplay-dispatch.csv"), "w"))
dispatch:write("frame,object,slot,state,owner,carrier,clock_minutes,clock_seconds\n")
local clock_calls = assert(io.open(join_path(capture_root, "gameplay-clock-calls.csv"), "w"))
clock_calls:write("frame,clock_minutes,clock_seconds,ball_state,phase\n")
local collision_calls = assert(io.open(join_path(capture_root, "gameplay-collision-calls.csv"), "w"))
collision_calls:write("frame,pc,current_object,ball_state,x_high,x_low,depth,height,rim_latch,outcome,owner,carrier,contact_timer,contact_limit,clock_gate,current_facing,owner_facing\n")
local score_calls = assert(io.open(join_path(capture_root, "gameplay-score-calls.csv"), "w"))
score_calls:write("frame,counter,ball_state,score_copy_a,score_copy_b,height,scoring_side,shot_kind\n")
local cpu_decisions = assert(io.open(join_path(capture_root, "gameplay-cpu-decisions.csv"), "w"))
cpu_decisions:write("frame,pc,current_object,state,animation,position,target,linked_object,linked_position,priority,global_phase,direction\n")
local control_calls = assert(io.open(join_path(capture_root, "gameplay-control-calls.csv"), "w"))
control_calls:write("frame,pc,current_object,receiver,switch_candidate,ball_state,owner,carrier,direction,mode,input,pressed,p2,p3,p4,p5,p6\n")
local counts = {formation = {}, toss_jump = {}, possession_award = {}, live = {}}
local previous_ball_state = -1
local previous_player_state = {}

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
if enable_pc_counts then
    memory.registerexecute(0x8000, 0x4000, function(address, size, value)
        local frame = emu.framecount()
        if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
            local phase = phase_for_frame(frame)
            counts[phase][address] = (counts[phase][address] or 0) + 1
        end
    end)
end
memory.registerexecute(0x9431, 1, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
        clock_calls:write(string.format("%d,%02X,%02X,%02X,%s\n", frame,
            memory.readbyte(0x0058), memory.readbyte(0x0057), memory.readbyte(0x0340),
            phase_for_frame(frame)))
    end
end)
local function record_collision_call(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
        local object = memory.readbyte(0x004B)
        local owner = memory.readbyte(0x005B)
        collision_calls:write(string.format("%d,%04X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
            frame, address, object, memory.readbyte(0x0340), memory.readbyte(0x0360),
            memory.readbyte(0x0370), memory.readbyte(0x03C0), memory.readbyte(0x0410),
            memory.readbyte(0x0490), memory.readbyte(0x0480), owner,
            memory.readbyte(0x0048), memory.readbyte(0x06A0 + object), memory.readbyte(0x0068),
            memory.readbyte(0x0025), memory.readbyte(0x0350 + object),
            owner < 0x10 and memory.readbyte(0x0350 + owner) or 0xFF))
    end
end
memory.registerexecute(0xB377, 1, record_collision_call)
memory.registerexecute(0xB473, 1, record_collision_call)
memory.registerexecute(0xB435, 1, record_collision_call)
memory.registerexecute(0xA347, 1, record_collision_call)
memory.registerexecute(0xA44B, 1, record_collision_call)
memory.registerexecute(0xAEDE, 1, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
        score_calls:write(string.format("%d,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
            frame, memory.readbyte(0x004A), memory.readbyte(0x0340),
            memory.readbyte(0x07F0), memory.readbyte(0x07F8),
            memory.readbyte(0x0410), memory.readbyte(0x0056),
            memory.readbyte(0x005F)))
    end
end)

local function record_cpu_decision(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
        local object = memory.readbyte(0x004B)
        local linked = memory.readbyte(0x0580 + object)
        cpu_decisions:write(string.format("%d,%04X,%02X,%02X,%02X,%02X%02X,%02X%02X,%02X,%02X%02X,%02X,%02X,%02X\n",
            frame, address, object, memory.readbyte(0x0340 + object),
            memory.readbyte(0x0350 + object), memory.readbyte(0x05C0 + object),
            memory.readbyte(0x05B0 + object), memory.readbyte(0x05E0 + object),
            memory.readbyte(0x05D0 + object), linked, memory.readbyte(0x05C0 + linked),
            memory.readbyte(0x05B0 + linked), memory.readbyte(0x004D),
            memory.readbyte(0x001A), memory.readbyte(0x0050)))
    end
end
for _, address in ipairs({0xD759, 0xD772, 0xD820, 0xD862, 0xD99A, 0xDA36, 0xDA38}) do
    memory.registerexecute(address, 1, record_cpu_decision)
end
for _, address in ipairs({0xA29D, 0xA314, 0xA329, 0xA33D, 0xA342,
                          0xAD41, 0xAD58, 0xAD6D, 0xADAD, 0xADBA}) do
    memory.registerexecute(address, 1, function(address, size, value)
        local frame = emu.framecount()
        if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
            local object = memory.readbyte(0x004B)
            control_calls:write(string.format("%d,%04X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
                frame, address, memory.readbyte(0x004B), memory.readbyte(0x0052), memory.readbyte(0x0061),
                memory.readbyte(0x0340), memory.readbyte(0x005B), memory.readbyte(0x0048),
                memory.readbyte(0x0050), memory.readbyte(0x002C),
                memory.readbyte(0x0670 + object), memory.readbyte(0x0680 + object),
                memory.readbyte(0x0342), memory.readbyte(0x0343), memory.readbyte(0x0344),
                memory.readbyte(0x0345), memory.readbyte(0x0346)))
        end
    end)
end

local capture_frames = {
    [2320]=true,[2340]=true,[2360]=true,[2380]=true,[2400]=true,[2420]=true,[2440]=true,
    [2460]=true,[2480]=true,[2500]=true,[2519]=true,[2520]=true,[2530]=true,[2531]=true,
    [2540]=true,[2550]=true,[2557]=true,[2560]=true,[2570]=true,[2580]=true,[2590]=true,
    [2600]=true,[2608]=true,[2614]=true,[2618]=true,[2620]=true,[2640]=true,
    [2660]=true,[2680]=true,[2684]=true,[2700]=true,[2723]=true,[2749]=true,[2760]=true,[2774]=true,
    [2770]=true,[2783]=true,[2929]=true,[2944]=true,[3004]=true,[3324]=true,
    [3501]=true,[3545]=true,[3553]=true,[3572]=true,[3600]=true,[3640]=true,
    [3680]=true,[3720]=true,[3800]=true,[3900]=true,[4000]=true,[4100]=true,[4200]=true,
    [11800]=true,[12000]=true,[12064]=true,[12096]=true,[12097]=true,[12100]=true,
    [12120]=true,[12160]=true,[12200]=true,[12300]=true,[12400]=true,[12412]=true,
    [12413]=true,[12420]=true,[12440]=true,[12480]=true,[12520]=true,[12600]=true,
    [12700]=true,[12800]=true,[12900]=true,[13000]=true,[13100]=true,[13200]=true,
    [45337]=true,[45338]=true,[45596]=true,[45597]=true,[45620]=true
}

local function write_state(frame)
    states:write(string.char(frame % 256, math.floor(frame / 256) % 256))
    local bytes = {}
    for address = 0, 0x07FF do bytes[#bytes + 1] = string.char(memory.readbyte(address)) end
    states:write(table.concat(bytes))
end

local function write_dispatch_changes(frame)
    local owner = memory.readbyte(0x005B)
    local carrier = memory.readbyte(0x0048)
    local minutes = memory.readbyte(0x0058)
    local seconds = memory.readbyte(0x0057)
    local ball_state = memory.readbyte(0x0340)
    if ball_state ~= previous_ball_state then
        dispatch:write(string.format("%d,ball,0,%02X,%02X,%02X,%02X,%02X\n",
            frame, ball_state, owner, carrier, minutes, seconds))
        previous_ball_state = ball_state
    end
    for slot = 2, 11 do
        local player_state = memory.readbyte(0x0340 + slot)
        if player_state ~= previous_player_state[slot] then
            dispatch:write(string.format("%d,player,%d,%02X,%02X,%02X,%02X,%02X\n",
                frame, slot, player_state, owner, carrier, minutes, seconds))
            previous_player_state[slot] = player_state
        end
    end
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
    if next_frame >= pass_frame and next_frame <= pass_end then
        input[pass_button] = true
        if pass_direction ~= "none" then input[pass_direction] = true end
    end
    if next_frame == inject_rim_frame then
        -- Controlled reverse-engineering probe for bank-0 $B473.  This is
        -- disabled by default and never contributes runtime data/assets.
        memory.writebyte(0x0340, 0x03)
        memory.writebyte(0x0360, 0x00)
        memory.writebyte(0x0370, 0x45)
        memory.writebyte(0x0380, 0x00)
        memory.writebyte(0x03C0, 0x58)
        memory.writebyte(0x03D0, 0x00)
        memory.writebyte(0x0410, 0x46)
        memory.writebyte(0x0420, 0x00)
        memory.writebyte(0x0390, 0x01)
        memory.writebyte(0x03A0, 0x00)
        memory.writebyte(0x03E0, 0x00)
        memory.writebyte(0x03F0, 0x00)
        memory.writebyte(0x0430, 0x01)
        memory.writebyte(0x0440, 0x00)
        memory.writebyte(0x0490, 0x00)
        memory.writebyte(0x0050, 0x08)
        memory.writebyte(0x005B, 0x07)
        memory.writebyte(0x0048, 0x07)
    end
    if next_frame == inject_contact_frame then
        -- Controlled probe for $B435 -> $9FA3.  A nonzero $0025 continues
        -- through $A44B possession transfer; zero plus equal facing makes
        -- $A347 take its whistle/dead-ball jump to $9645 instead.
        local player = 0x03
        memory.writebyte(0x0340, 0x01)
        memory.writebyte(0x005B, 0x07)
        memory.writebyte(0x0048, 0x07)
        memory.writebyte(0x0050, 0x08)
        memory.writebyte(0x0068, 0x03)
        memory.writebyte(0x0360, 0x00)
        memory.writebyte(0x0370, 0x80)
        memory.writebyte(0x03C0, 0x58)
        memory.writebyte(0x0410, 0x10)
        memory.writebyte(0x0360 + 0x07, 0x00)
        memory.writebyte(0x0370 + 0x07, 0x80)
        memory.writebyte(0x03C0 + 0x07, 0x58)
        memory.writebyte(0x0410 + 0x07, 0x10)
        memory.writebyte(0x0340 + 0x07, 0x3B)
        memory.writebyte(0x0340 + player, 0x3B)
        memory.writebyte(0x0350 + player, memory.readbyte(0x0350 + 0x07))
        memory.writebyte(0x0360 + player, 0x00)
        memory.writebyte(0x0370 + player, 0x80)
        memory.writebyte(0x03C0 + player, 0x58)
        memory.writebyte(0x0410 + player, 0x10)
        memory.writebyte(0x0580 + player, 0x07)
        memory.writebyte(0x0690 + player, 0x01)
        memory.writebyte(0x06A0 + player, 0x00)
        if inject_contact_clock_gate >= 0 then
            memory.writebyte(0x0025, inject_contact_clock_gate)
        end
    end
    if next_frame == inject_basket_frame then
        -- Controlled $AE25->$B377 classifier probe.  $AE25 increments $04F0
        -- first; injecting $FF proves B377's wrap/arming return, while 0
        -- classifies the requested result on this frame.
        local hoop_x = 0x48
        memory.writebyte(0x0340, 0x05)
        memory.writebyte(0x003B, 0x00)
        memory.writebyte(0x0050, 0x08)
        memory.writebyte(0x005B, 0x07)
        memory.writebyte(0x0340 + 0x07, 0x25)
        memory.writebyte(0x0360, 0x00)
        memory.writebyte(0x0370, hoop_x +
            (inject_basket_result == 1 and 1 or inject_basket_result + 1))
        memory.writebyte(0x03C0, 0x58)
        memory.writebyte(0x0410, 0x35)
        memory.writebyte(0x0420, 0x00)
        memory.writebyte(0x0390, 0x00)
        memory.writebyte(0x03A0, 0x00)
        memory.writebyte(0x03E0, 0x00)
        memory.writebyte(0x03F0, 0x00)
        memory.writebyte(0x0430, 0x00)
        memory.writebyte(0x0440, 0x00)
        memory.writebyte(0x0480, 0x00)
        memory.writebyte(0x0490, 0x00)
        memory.writebyte(0x04F0, inject_basket_counter)
    end
    if next_frame == inject_ball_state_frame then
        -- Controlled dispatcher probe, disabled by default.  It verifies that
        -- unobserved state $0C shares $0B's $ACAB projection handler.
        memory.writebyte(0x0340, inject_ball_state)
        memory.writebyte(0x0410, 0x46)
        memory.writebyte(0x0420, 0x7B)
        memory.writebyte(0x0390, 0x01)
        memory.writebyte(0x03A0, 0x23)
        memory.writebyte(0x03E0, 0xFF)
        memory.writebyte(0x03F0, 0xBB)
        memory.writebyte(0x0430, 0x00)
        memory.writebyte(0x0440, 0x67)
    end
    if next_frame == inject_loose_launch_frame then
        -- Controlled $AF72 branch probe for outcomes $02/$03/$04.
        memory.writebyte(0x0340, 0x08)
        memory.writebyte(0x0480, inject_loose_outcome)
        memory.writebyte(0x0470, 0x20)
        memory.writebyte(0x0390, 0x00)
        memory.writebyte(0x03A0, 0x80)
        memory.writebyte(0x03E0, 0x00)
        memory.writebyte(0x03F0, 0x40)
        memory.writebyte(0x0410, 0x37)
        memory.writebyte(0x0420, 0x55)
    end
    if next_frame == inject_player_state_frame then
        -- Controlled player-dispatch probe. Seed all three motion vectors so
        -- $8297's bare RTS and $8460->$B503 can be distinguished dynamically.
        local player = inject_player_slot
        memory.writebyte(0x0340 + player, inject_player_state)
        memory.writebyte(0x0390 + player, 0x01)
        memory.writebyte(0x03A0 + player, 0x23)
        memory.writebyte(0x03E0 + player, 0xFE)
        memory.writebyte(0x03F0 + player, 0xDC)
        memory.writebyte(0x0430 + player, 0x03)
        memory.writebyte(0x0440 + player, 0x45)
        if inject_player_hold_case ~= 0 then
            -- Controlled $8EE2 formation-ready standard/alternate branches.
            for slot = 0x02, 0x0B do
                memory.writebyte(0x0340 + slot, 0x37)
                memory.writebyte(0x0690 + slot, (slot - 0x02) % 5)
            end
            memory.writebyte(0x0340 + player, 0x30)
            memory.writebyte(0x04F0 + player, 0x0A)
            memory.writebyte(0x0050,
                inject_player_hold_case == 2 and 0x48 or 0x08)
            memory.writebyte(0x002C,
                inject_player_hold_case == 3 and 0x01 or 0x00)
            memory.writebyte(0x0052, 0x00)
            memory.writebyte(0x0056, 0x01)
            memory.writebyte(0x0340, 0x0B)
        end
        if inject_player_pair_case ~= 0 then
            -- Controlled $8A16/$8A98->$9102 paired-player contact.
            local paired = 0x02
            memory.writebyte(0x0340 + player,
                inject_player_pair_case == 2 and 0x22 or 0x20)
            memory.writebyte(0x0580 + player, paired)
            memory.writebyte(0x0360 + player, 0x00)
            memory.writebyte(0x0370 + player, 0x80)
            memory.writebyte(0x03C0 + player, 0x58)
            memory.writebyte(0x0490 + paired,
                inject_player_pair_case == 1 and 0x58 or 0x20)
            memory.writebyte(0x04A0 + paired, 0x80)
            memory.writebyte(0x04B0 + paired, 0x00)
            if inject_player_pair_case == 3 then
                memory.writebyte(0x0340 + paired, 0x03)
            end
            memory.writebyte(0x0340, 0x0B)
        end
        if inject_player_inbound_case ~= 0 then
            -- Controlled $8FE0 release countdown installed by $9018.
            memory.writebyte(0x0340 + player, 0x31)
            memory.writebyte(0x04E0 + player,
                inject_player_inbound_case == 2 and 0x00 or 0x08)
            memory.writebyte(0x04F0 + player, 0x0A)
            memory.writebyte(0x0300 + player, 0x40)
            memory.writebyte(0x0340, 0x0B)
            memory.writebyte(0x0048, player)
        end
        if inject_player_jump_case ~= 0 then
            -- Controlled $8AF4->$8B12->$9ABD probe.  The nonzero global
            -- collision gate isolates the signed height script and landing.
            memory.writebyte(0x0340 + player, 0x23)
            memory.writebyte(0x0410 + player, 0x10)
            memory.writebyte(0x0420 + player, 0x55)
            memory.writebyte(0x0056, inject_player_jump_case == 1 and 0x01 or 0x00)
            memory.writebyte(0x005B, 0xFF)
            memory.writebyte(0x0048, 0xFF)
            memory.writebyte(0x0340, 0x0B)
            memory.writebyte(0x0370, 0x00)
            memory.writebyte(0x03C0, 0x00)
            memory.writebyte(0x0410, 0x00)
        end
        if inject_player_route_case ~= 0 then
            -- Force $81A2's same-region, not-at-target branch. Slot $0B is
            -- the role-zero reference selected by $9097 on this possession side.
            memory.writebyte(0x0340 + player, 0x39)
            memory.writebyte(0x0690 + player, 0x04)
            memory.writebyte(0x05B0 + player, 0xEC)
            memory.writebyte(0x05C0 + player, 0x00)
            memory.writebyte(0x05D0 + player, 0x8C)
            memory.writebyte(0x05E0 + player, 0x00)
            memory.writebyte(0x0360 + player, 0x00)
            memory.writebyte(0x0370 + player, 0xC6)
            memory.writebyte(0x03C0 + player, 0x79)
            memory.writebyte(0x0690 + 0x0B, 0x00)
            memory.writebyte(0x05B0 + 0x0B, 0xEB)
            memory.writebyte(0x05C0 + 0x0B, 0x00)
            memory.writebyte(0x0360 + 0x0B, 0x00)
            memory.writebyte(0x0370 + 0x0B, 0xB9)
            memory.writebyte(0x03C0 + 0x0B, 0x79)
            memory.writebyte(0x001A, 0x01)
            memory.writebyte(0x0050, 0x08)
            if inject_player_route_case == 2 then
                -- $8A3A/$D978 arrival: current and target packed bytes match;
                -- $0600 must clear as the state returns $21->$20.
                memory.writebyte(0x0340 + player, 0x21)
                memory.writebyte(0x05D0 + player, 0xEC)
                memory.writebyte(0x0600 + player, 0x55)
                memory.writebyte(0x0340, 0x0B)
                memory.writebyte(0x005B, 0xFF)
                memory.writebyte(0x0048, 0xFF)
            end
        end
    end
    joypad.set(1, input)
    emu.frameadvance()
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end then
        local snd = sound.get().rp2a03
        write_state(frame)
        write_dispatch_changes(frame)
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
dispatch:close()
clock_calls:close()
collision_calls:close()
score_calls:close()
cpu_decisions:close()
control_calls:close()
local count_file = assert(io.open(join_path(capture_root, "tipoff-pc-counts.csv"), "w"))
count_file:write("phase,address,count\n")
for phase, phase_counts in pairs(counts) do
    for address, count in pairs(phase_counts) do
        count_file:write(string.format("%s,%04X,%d\n", phase, address, count))
    end
end
count_file:close()
emu.exit()
