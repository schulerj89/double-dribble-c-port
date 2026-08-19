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
local move_start = tonumber(os.getenv("DD_MOVE_START") or "-1")
local move_end = tonumber(os.getenv("DD_MOVE_END") or tostring(move_start))
local move_direction = os.getenv("DD_MOVE_DIRECTION") or "none"
local inject_physics_boundary = os.getenv("DD_INJECT_PHYSICS_BOUNDARY") or "none"
local enable_pc_counts = os.getenv("DD_ENABLE_PC_COUNTS") ~= "0"
local inject_rim_frame = tonumber(os.getenv("DD_INJECT_RIM_FRAME") or "-1")
local inject_contact_frame = tonumber(os.getenv("DD_INJECT_CONTACT_FRAME") or "-1")
local inject_contact_clock_gate = tonumber(os.getenv("DD_INJECT_CONTACT_CLOCK_GATE") or "-1")
local inject_contact_level = tonumber(os.getenv("DD_INJECT_CONTACT_LEVEL") or "-1")
local inject_contact_limit = tonumber(os.getenv("DD_INJECT_CONTACT_LIMIT") or "-1")
local inject_contact_phase = tonumber(os.getenv("DD_INJECT_CONTACT_PHASE") or "-1")
local inject_contact_ball_state = tonumber(os.getenv("DD_INJECT_CONTACT_BALL_STATE") or "-1")
local inject_contact_pair = os.getenv("DD_INJECT_CONTACT_PAIR") or "owner"
local inject_user_free_throw_frame = tonumber(os.getenv("DD_INJECT_USER_FREE_THROW_FRAME") or "-1")
local inject_cpu_free_throw_frame = tonumber(os.getenv("DD_INJECT_CPU_FREE_THROW_FRAME") or "-1")
local inject_cpu_free_throw_level = tonumber(os.getenv("DD_INJECT_CPU_FREE_THROW_LEVEL") or "-1")
local inject_cpu_free_throw_phase = tonumber(os.getenv("DD_INJECT_CPU_FREE_THROW_PHASE") or "-1")
local inject_cpu_free_throw_aim = tonumber(os.getenv("DD_INJECT_CPU_FREE_THROW_AIM") or "-1")
local inject_cpu_free_throw_timer = tonumber(os.getenv("DD_INJECT_CPU_FREE_THROW_TIMER") or "-1")
local inject_cpu_free_throw_gate = tonumber(os.getenv("DD_INJECT_CPU_FREE_THROW_GATE") or "-1")
local inject_basket_frame = tonumber(os.getenv("DD_INJECT_BASKET_FRAME") or "-1")
local inject_basket_result = tonumber(os.getenv("DD_INJECT_BASKET_RESULT") or "1")
local inject_basket_counter = tonumber(os.getenv("DD_INJECT_BASKET_COUNTER") or "0")
local inject_basket_shot_kind = tonumber(os.getenv("DD_INJECT_BASKET_SHOT_KIND") or "0")
local inject_block_frame = tonumber(os.getenv("DD_INJECT_BLOCK_FRAME") or "-1")
local inject_user_block_frame = tonumber(os.getenv("DD_INJECT_USER_BLOCK_FRAME") or "-1")
local inject_user_steal_frame = tonumber(os.getenv("DD_INJECT_USER_STEAL_FRAME") or "-1")
local inject_user_steal_button = os.getenv("DD_INJECT_USER_STEAL_BUTTON") or "A"
local inject_user_steal_lock = tonumber(os.getenv("DD_INJECT_USER_STEAL_LOCK") or "-1")
local inject_user_steal_gate = tonumber(os.getenv("DD_INJECT_USER_STEAL_GATE") or "-1")
local inject_user_steal_ball_state = tonumber(os.getenv("DD_INJECT_USER_STEAL_BALL_STATE") or "-1")
local inject_user_steal_paired_action = tonumber(os.getenv("DD_INJECT_USER_STEAL_PAIRED_ACTION") or "-1")
local inject_user_steal_collision = os.getenv("DD_INJECT_USER_STEAL_COLLISION") or "hit"
local inject_user_steal_foul = os.getenv("DD_INJECT_USER_STEAL_FOUL") == "1"
local inject_user_steal_same_player = os.getenv("DD_INJECT_USER_STEAL_SAME_PLAYER") == "1"
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
local inject_inbound_rule_frame = tonumber(os.getenv("DD_INJECT_INBOUND_RULE_FRAME") or "-1")
local inject_inbound_rule_case = tonumber(os.getenv("DD_INJECT_INBOUND_RULE_CASE") or "0")
local inject_exceptional_reason_frame = tonumber(os.getenv("DD_INJECT_EXCEPTIONAL_REASON_FRAME") or "-1")
local inject_shot_kind_case = tonumber(os.getenv("DD_INJECT_SHOT_KIND_CASE") or "0")
local user_shot_depth = tonumber(os.getenv("DD_USER_SHOT_DEPTH") or "-1")
local user_shot_x = tonumber(os.getenv("DD_USER_SHOT_X") or "-1")
local user_position_frame = tonumber(os.getenv("DD_USER_POSITION_FRAME") or "-1")
local score_audio_freeze_frame = tonumber(os.getenv("DD_SCORE_AUDIO_FREEZE_FRAME") or "-1")
local cpu_region2_probe = os.getenv("DD_CPU_REGION2_PROBE") or "none"

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
local level_contact = assert(io.open(join_path(capture_root, "gameplay-level-contact.csv"), "w"))
level_contact:write("frame,bank,pc,current_object,level,contact_limit,tracking_limit,contact_lock,score_gate,direction,global_phase,pair,controlled,owner,role,ball_state,player_state,contact_counter,tracked_zone,tracking_age\n")
local score_calls = assert(io.open(join_path(capture_root, "gameplay-score-calls.csv"), "w"))
score_calls:write("frame,counter,ball_state,score_copy_a,score_copy_b,height,scoring_side,shot_kind\n")
local cpu_decisions = assert(io.open(join_path(capture_root, "gameplay-cpu-decisions.csv"), "w"))
cpu_decisions:write("frame,pc,current_object,state,animation,position,target,linked_object,linked_position,priority,global_phase,direction\n")
local cpu_region2 = assert(io.open(join_path(capture_root, "gameplay-cpu-region2.csv"), "w"))
cpu_region2:write("frame,pc,a,x,y,scratch2f,scratch30,scratch31,scratch32,current,state,position,target,role0,role0_position,paired,paired_position,direction,global_phase,decision_timer,velocity_x,velocity_depth,probe\n")
local physics_calls = assert(io.open(join_path(capture_root, "gameplay-physics-calls.csv"), "w"))
physics_calls:write("frame,pc,current_object,state,facing,x,velocity_x,depth,velocity_depth,height,vertical_base,elapsed,curve,duration,packed,target,priority,global_phase\n")
local control_calls = assert(io.open(join_path(capture_root, "gameplay-control-calls.csv"), "w"))
control_calls:write("frame,pc,current_object,receiver,switch_candidate,ball_state,owner,carrier,direction,mode,input,pressed,p2,p3,p4,p5,p6\n")
local block_calls = assert(io.open(join_path(capture_root, "gameplay-block-calls.csv"), "w"))
block_calls:write("frame,pc,current_object,player_state,ball_state,owner,carrier,input,pressed,player_x,player_depth,player_height,ball_x,ball_depth,ball_height,paired,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11\n")
local user_contest_calls = assert(io.open(join_path(capture_root, "gameplay-user-contest-calls.csv"), "w"))
user_contest_calls:write("frame,pc,current_object,player_state,ball_state,acquisition_mode,global_gate,owner,carrier,input,pressed,player_x,player_height,ball_x,ball_height,script_low,script_high,script_value,paired,paired_state\n")
local shot_kind_calls = assert(io.open(join_path(capture_root, "gameplay-shot-kind-calls.csv"), "w"))
shot_kind_calls:write("frame,pc,current_object,ball_state,ball_x_high,ball_x_low,ball_depth,depth_offset,boundary_index,boundary_value,shot_kind\n")
local sfx_calls = assert(io.open(join_path(capture_root, "gameplay-sfx-calls.csv"), "w"))
sfx_calls:write("frame,event,current_object,ball_state,shot_kind\n")
local exceptional_calls = assert(io.open(join_path(capture_root, "gameplay-exceptional-calls.csv"), "w"))
exceptional_calls:write("frame,current,clock_gate,ball_state,input,current_target,current_facing,defender,defender_state,defender_target,defender_facing\n")
local shot_animation = assert(io.open(join_path(capture_root, "gameplay-shot-animation.csv"), "w"))
shot_animation:write("frame,pc,current_object,player_state,facing,metasprite,animation_phase,player_height,script_low,script_high,script_value,release_gate,ball_state,ball_owner,carrier,ball_x,ball_depth,ball_height,outcome\n")
local free_throw_calls = assert(io.open(join_path(capture_root, "gameplay-free-throw-calls.csv"), "w"))
free_throw_calls:write("frame,pc,current_object,player_state,ball_state,owner,carrier,phase_counter,shot_timer,dead_timer,dead_phase,role,target,facing,input,coarse_timer,aim,aim_direction\n")
local counts = {formation = {}, toss_jump = {}, possession_award = {}, live = {}}
local bank_counts = {}
local previous_ball_state = -1
local previous_player_state = {}
local physics_boundary_pending = inject_physics_boundary ~= "none"

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
        if frame >= trace_start and frame <= trace_end then
            local bank = current_switch_bank()
            bank_counts[bank] = bank_counts[bank] or {}
            bank_counts[bank][address] = (bank_counts[bank][address] or 0) + 1
            if bank == 0 then
                local phase = phase_for_frame(frame)
                counts[phase][address] = (counts[phase][address] or 0) + 1
            end
        end
    end)
    memory.registerexecute(0xC000, 0x4000, function(address, size, value)
        local frame = emu.framecount()
        if frame >= trace_start and frame <= trace_end then
            bank_counts[7] = bank_counts[7] or {}
            bank_counts[7][address] = (bank_counts[7][address] or 0) + 1
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
for _, address in ipairs({
    0x852F, 0x8534, 0x85EF, 0x8603, 0x860A, 0x862A, 0x8636,
    0x8682, 0x8694, 0x86C3, 0x872F, 0x8774, 0x87C0, 0x87F2,
    0x87F7, 0x8832, 0x883A, 0x8841, 0x8845, 0x884C, 0x884F,
    0x8882, 0x8887, 0x88BE, 0x88CD, 0x88DE,
    0x8902, 0x891C, 0x894C, 0x8957, 0x8974, 0x897A, 0x897F
}) do
    memory.registerexecute(address, 1, function(address, size, value)
        local frame = emu.framecount()
        if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
            local object = memory.readbyte(0x004B)
            free_throw_calls:write(string.format(
                "%d,%04X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X%02X,%02X,%02X,%02X,%02X\n",
                frame, address, object, memory.readbyte(0x0340 + object),
                memory.readbyte(0x0340), memory.readbyte(0x005B), memory.readbyte(0x0048),
                memory.readbyte(0x0066), memory.readbyte(0x0067),
                memory.readbyte(0x0064), memory.readbyte(0x0065),
                memory.readbyte(0x0690 + object), memory.readbyte(0x05C0 + object),
                memory.readbyte(0x05B0 + object), memory.readbyte(0x0350 + object),
                memory.readbyte(0x0680 + object), memory.readbyte(0x06B1),
                memory.readbyte(0x033C), memory.readbyte(0x051C)))
        end
    end)
end
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
local function record_level_contact(address, size, value)
    local frame = emu.framecount()
    local bank = current_switch_bank()
    local bank0_root = address == 0x91A6 or address == 0x91F3 or
        address == 0x9200 or address == 0x9FA3 or address == 0x9FFC or
        address == 0xA009 or address == 0x8A57 or address == 0x8A7D or
        address == 0x8A90
    local bank1_root = address == 0xA593 or address == 0xA5B7 or
        address == 0xA631 or address == 0xA637 or address == 0xAC04 or
        address == 0xAC1F or address == 0xAC25 or address == 0xAC2E
    if frame >= trace_start and frame <= trace_end and
       ((bank == 0 and bank0_root) or (bank == 1 and bank1_root)) then
        local object = memory.readbyte(0x004B)
        level_contact:write(string.format(
            "%d,%d,%04X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
            frame, bank, address, object, memory.readbyte(0x07E8),
            memory.readbyte(0x0068), memory.readbyte(0x006C),
            memory.readbyte(0x001D), memory.readbyte(0x0056),
            memory.readbyte(0x0050), memory.readbyte(0x001A),
            memory.readbyte(0x0580 + object), memory.readbyte(0x0046),
            memory.readbyte(0x005B), memory.readbyte(0x0690 + object),
            memory.readbyte(0x0340), memory.readbyte(0x0340 + object),
            memory.readbyte(0x06A0 + object), memory.readbyte(0x05F0 + object),
            memory.readbyte(0x0600 + object)))
    end
end
for _, address in ipairs({
    0x91A6, 0x91F3, 0x9200, 0x9FA3, 0x9FFC, 0xA009,
    0x8A57, 0x8A7D, 0x8A90, 0xA593, 0xA5B7, 0xA631, 0xA637,
    0xAC04, 0xAC1F, 0xAC25, 0xAC2E
}) do
    memory.registerexecute(address, 1, record_level_contact)
end
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
memory.registerexecute(0xC141, 1, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end then
        sfx_calls:write(string.format("%d,%02X,%02X,%02X,%02X\n", frame,
            memory.getregister("a"), memory.readbyte(0x004B),
            memory.readbyte(0x0340), memory.readbyte(0x005F)))
    end
end)

local function record_shot_animation(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
        local object = memory.readbyte(0x004B)
        local script_low = object < 0x10 and memory.readbyte(0x0500 + object) or 0xFF
        local script_high = object < 0x10 and memory.readbyte(0x0510 + object) or 0xFF
        local script_value = 0xFF
        if script_high >= 0x80 and script_high < 0xC0 then
            script_value = memory.readbyte(script_high * 0x100 + script_low)
        end
        shot_animation:write(string.format(
            "%d,%04X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X%02X,%02X,%02X,%02X\n",
            frame, address, object,
            object < 0x10 and memory.readbyte(0x0340 + object) or 0xFF,
            object < 0x10 and memory.readbyte(0x0350 + object) or 0xFF,
            object < 0x10 and memory.readbyte(0x0300 + object) or 0xFF,
            object < 0x10 and memory.readbyte(0x0450 + object) or 0xFF,
            object < 0x10 and memory.readbyte(0x0410 + object) or 0xFF,
            script_low, script_high, script_value,
            object < 0x10 and memory.readbyte(0x04E0 + object) or 0xFF,
            memory.readbyte(0x0340), memory.readbyte(0x005B), memory.readbyte(0x0048),
            memory.readbyte(0x0360), memory.readbyte(0x0370),
            memory.readbyte(0x03C0), memory.readbyte(0x0410), memory.readbyte(0x0480)))
    end
end
for _, address in ipairs({0xAA75, 0xA504, 0xA896, 0x9ABD, 0xB189, 0xAE25, 0xB377, 0xAEDE, 0xAF72}) do
    memory.registerexecute(address, 1, record_shot_animation)
end

local function record_shot_kind(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
        local object = memory.readbyte(0x004B)
        if address == 0xA7EA and inject_shot_kind_case ~= 0 then
            if object < 0x07 and (inject_shot_kind_case == 1 or inject_shot_kind_case == 2) then
                memory.writebyte(0x0360, 0x01)
                memory.writebyte(0x0370, inject_shot_kind_case == 1 and 0x41 or 0x40)
                memory.writebyte(0x03C0, 0x58)
            elseif object >= 0x07 and (inject_shot_kind_case == 3 or inject_shot_kind_case == 4) then
                memory.writebyte(0x0360, 0x00)
                memory.writebyte(0x0370, inject_shot_kind_case == 3 and 0xC0 or 0xC1)
                memory.writebyte(0x03C0, 0x58)
            end
        end
        local depth = memory.readbyte(0x03C0)
        local depth_offset = (depth - 0x26) % 0x100
        local boundary_index = 0xFF
        local boundary_value = 0xFF
        if depth_offset < 0x5C then
            boundary_index = math.floor(depth_offset / 4)
            boundary_value = memory.readbyte(0xA834 + boundary_index)
        end
        shot_kind_calls:write(string.format(
            "%d,%04X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
            frame, address, object, memory.readbyte(0x0340),
            memory.readbyte(0x0360), memory.readbyte(0x0370), depth,
            depth_offset, boundary_index, boundary_value, memory.readbyte(0x005F)))
    end
end
for _, address in ipairs({0xA7EA, 0xA82A, 0xA82E, 0xA833}) do
    memory.registerexecute(address, 1, record_shot_kind)
end

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
for _, address in ipairs({
    0xD759, 0xD772, 0xD7CC, 0xD7DE, 0xD820, 0xD834, 0xD857,
    0xD862, 0xD885, 0xD8AA, 0xD8B0, 0xD8F1, 0xD8FA, 0xD92F,
    0xD99A, 0xDA36, 0xDA38
}) do
    memory.registerexecute(address, 1, record_cpu_decision)
end

-- Focused fixed-bank `$D77B-$D8B0` evidence. `$9097` selects role zero from
-- original slots $02-$06 or $07-$0B according to `$0050.3`; it does not scan
-- `$85/$86/$87`. Optional controlled probes alter only the compared packed
-- byte at `$D795`, after `$9097` has returned, to force each rejection edge.
local cpu_region2_injected = false
local function cpu_region2_mirror(packed)
    return bit.bor(bit.band(packed, 0xE0), 0x1F - bit.band(packed, 0x1F))
end
local function cpu_region2_role_zero()
    local direction = memory.readbyte(0x0050)
    local first = bit.band(direction, 0x08) ~= 0 and 0x02 or 0x07
    for object = first, first + 4 do
        if memory.readbyte(0x0690 + object) == 0 then return object end
    end
    return first
end

local function record_cpu_region2(address, size, value)
    local frame = emu.framecount()
    if frame < trace_start or frame > trace_end or current_switch_bank() ~= 0 then return end
    local current = memory.readbyte(0x004B)
    local paired = memory.readbyte(0x0580 + current)
    local role0 = cpu_region2_role_zero()
    local scratch31 = memory.readbyte(0x0031)
    if address == 0xD772 and cpu_region2_probe == "mirror" and
       not cpu_region2_injected then
        -- `$D978` has already accepted equality. Mirror both packed bytes and
        -- set `$0050.6` before `$AC2A`, preserving the same logical region
        -- while exercising `$AC64/$AC5C`'s opposite-direction output.
        memory.writebyte(0x0050, bit.bor(memory.readbyte(0x0050), 0x40))
        memory.writebyte(0x05B0 + current,
            cpu_region2_mirror(memory.readbyte(0x05B0 + current)))
        memory.writebyte(0x05D0 + current,
            cpu_region2_mirror(memory.readbyte(0x05D0 + current)))
        cpu_region2_injected = true
    end
    if address == 0xD795 and not cpu_region2_injected then
        if cpu_region2_probe == "role0" then
            memory.writebyte(0x05B0 + role0, scratch31)
            cpu_region2_injected = true
        elseif cpu_region2_probe == "paired" then
            -- The natural opening trace links the carrier directly to the
            -- same role-zero object. Move only the controlled probe's link
            -- to its adjacent teammate so the first comparison misses and
            -- `$D7A2-$D7A7` proves the distinct paired-player rejection.
            if paired == role0 then
                paired = role0 == 0x02 and 0x03 or 0x02
                memory.writebyte(0x0580 + current, paired)
            end
            memory.writebyte(0x05B0 + paired, scratch31)
            cpu_region2_injected = true
        end
    end
    cpu_region2:write(string.format(
        "%d,%04X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X%02X,%02X%02X,%02X,%02X%02X,%02X,%02X%02X,%02X,%02X,%02X,%02X%02X,%02X%02X,%s\n",
        frame, address, memory.getregister("a"), memory.getregister("x"),
        memory.getregister("y"), memory.readbyte(0x002F), memory.readbyte(0x0030),
        scratch31, memory.readbyte(0x0032), current,
        memory.readbyte(0x0340 + current), memory.readbyte(0x05C0 + current),
        memory.readbyte(0x05B0 + current), memory.readbyte(0x05E0 + current),
        memory.readbyte(0x05D0 + current), role0, memory.readbyte(0x05C0 + role0),
        memory.readbyte(0x05B0 + role0), paired, memory.readbyte(0x05C0 + paired),
        memory.readbyte(0x05B0 + paired), memory.readbyte(0x0050),
        memory.readbyte(0x001A), memory.readbyte(0x04F0 + current),
        memory.readbyte(0x0390 + current), memory.readbyte(0x03A0 + current),
        memory.readbyte(0x03E0 + current), memory.readbyte(0x03F0 + current),
        cpu_region2_probe))
end
for _, address in ipairs({
    0xD772, 0xD77B, 0xD795, 0xD7A2, 0xD7A9, 0xD7B1, 0xD7C5, 0xD857, 0xD8B0,
    0x8BF8, 0xD98A, 0xD98D
}) do
    memory.registerexecute(address, 1, record_cpu_region2)
end
local function record_physics_call(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
        local object = memory.readbyte(0x004B)
        if object < 0x10 then
            local tracked = object
            if address == 0xB0AB or address == 0xB11E or address == 0xB189 or
               address == 0xB29C or address == 0xB2EE or address == 0xB376 then
                tracked = 0
            end
            -- Controlled primitive-only rejection probes.  Injection occurs
            -- at the original helper entry after its caller has installed a
            -- real object/velocity, so `$9CA0/$9CF6` executes the complete
            -- unchanged accept/reject path and the existing return hooks
            -- record its result.
            if physics_boundary_pending and address == 0x9CA0 and
               (inject_physics_boundary == "x-upper" or
                inject_physics_boundary == "x-lower") then
                memory.writebyte(0x0360 + tracked,
                    inject_physics_boundary == "x-upper" and 0x01 or 0x00)
                memory.writebyte(0x0370 + tracked,
                    inject_physics_boundary == "x-upper" and 0xF1 or 0x10)
                memory.writebyte(0x0380 + tracked, 0x80)
                memory.writebyte(0x0390 + tracked,
                    inject_physics_boundary == "x-upper" and 0x01 or 0xFF)
                memory.writebyte(0x03A0 + tracked, 0x00)
                physics_boundary_pending = false
            elseif physics_boundary_pending and address == 0x9CF6 and
                   (inject_physics_boundary == "depth-upper" or
                    inject_physics_boundary == "depth-lower") then
                memory.writebyte(0x03B0 + tracked, 0x00)
                memory.writebyte(0x03C0 + tracked,
                    inject_physics_boundary == "depth-upper" and 0x98 or 0x05)
                memory.writebyte(0x03D0 + tracked, 0x80)
                memory.writebyte(0x03E0 + tracked,
                    inject_physics_boundary == "depth-upper" and 0x01 or 0xFF)
                memory.writebyte(0x03F0 + tracked, 0x00)
                physics_boundary_pending = false
            end
            physics_calls:write(string.format(
                "%d,%04X,%02X,%02X,%02X,%02X%02X%02X,%02X%02X,%02X%02X%02X,%02X%02X,%02X%02X,%02X%02X,%02X,%02X,%02X,%02X%02X,%02X%02X,%02X,%02X\n",
                frame, address, object, memory.readbyte(0x0340 + tracked),
                memory.readbyte(0x0350 + tracked),
                memory.readbyte(0x0360 + tracked), memory.readbyte(0x0370 + tracked),
                memory.readbyte(0x0380 + tracked), memory.readbyte(0x0390 + tracked),
                memory.readbyte(0x03A0 + tracked), memory.readbyte(0x03B0 + tracked),
                memory.readbyte(0x03C0 + tracked), memory.readbyte(0x03D0 + tracked),
                memory.readbyte(0x03E0 + tracked), memory.readbyte(0x03F0 + tracked),
                memory.readbyte(0x0410 + tracked), memory.readbyte(0x0420 + tracked),
                memory.readbyte(0x0430 + tracked), memory.readbyte(0x0440 + tracked),
                memory.readbyte(0x004A), memory.readbyte(0x004C),
                memory.readbyte(0x04B0 + tracked),
                memory.readbyte(0x05C0 + tracked), memory.readbyte(0x05B0 + tracked),
                memory.readbyte(0x05E0 + tracked), memory.readbyte(0x05D0 + tracked),
                memory.readbyte(0x004D), memory.readbyte(0x001A)))
        end
    end
end
for _, address in ipairs({
    0x9B84, 0x9BAF, 0x9CA0, 0x9CEB, 0x9CF5, 0x9CF6, 0x9D22, 0x9D2C,
    0x9E2D, 0x9E4B, 0xA84C, 0xABCD, 0xAC29, 0xB0AB, 0xB11E,
    0xB167, 0xB17E, 0xB188, 0xB189, 0xB29C, 0xB2EE, 0xB376
}) do
    memory.registerexecute(address, 1, record_physics_call)
end
for _, address in ipairs({0xA129, 0xA1C9, 0xA21F, 0xA230, 0xA241,
                          0xA29D, 0xA314, 0xA329, 0xA33D, 0xA342,
                          0xA780, 0xA78E, 0xA795, 0xA7B6, 0xA7C6,
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
for _, address in ipairs({0xA3E2, 0xA504, 0xA607, 0x9102, 0x9139,
                          0x8AF4, 0x8B12, 0x8B21, 0x8B27, 0x8B33, 0x8B44,
                          0x9208, 0xA6C3, 0xAE0C, 0xAE25}) do
    memory.registerexecute(address, 1, function(address, size, value)
        local frame = emu.framecount()
        if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
            local object = memory.readbyte(0x004B)
            local states = {}
            for slot = 2, 11 do states[#states + 1] = memory.readbyte(0x0340 + slot) end
            block_calls:write(string.format(
                "%d,%04X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
                frame, address, object, memory.readbyte(0x0340 + object), memory.readbyte(0x0340),
                memory.readbyte(0x005B), memory.readbyte(0x0048),
                memory.readbyte(0x0670 + object), memory.readbyte(0x0680 + object),
                memory.readbyte(0x0370 + object), memory.readbyte(0x03C0 + object),
                memory.readbyte(0x0410 + object), memory.readbyte(0x0370),
                memory.readbyte(0x03C0), memory.readbyte(0x0410),
                memory.readbyte(0x0580 + object), unpack(states)))
        end
    end)
end
for _, address in ipairs({0xA3E2, 0xA402, 0xA40A, 0xA426, 0xA42D,
                          0xA434, 0xA439, 0xA43F, 0xA444, 0xA44B,
                          0xA460, 0xA478, 0xA607, 0x9645,
                          0xA638, 0xA651, 0xA656, 0xA672, 0xA67D,
                          0xA68A, 0xA693, 0xA6AD, 0xA6B8, 0xA6C3}) do
    memory.registerexecute(address, 1, function(address, size, value)
        local frame = emu.framecount()
        if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
            local object = memory.readbyte(0x004B)
            if address == 0xA3E2 and frame == inject_user_steal_frame and object == 0x02 then
                if inject_user_steal_lock >= 0 then memory.writebyte(0x001D, inject_user_steal_lock) end
                if inject_user_steal_gate >= 0 then memory.writebyte(0x0056, inject_user_steal_gate) end
            end
            local paired = memory.readbyte(0x0580 + object)
            local script_low = memory.readbyte(0x0500 + object)
            local script_high = memory.readbyte(0x0510 + object)
            local script_value = 0xFF
            local script_address = script_high * 0x100 + script_low
            if script_address >= 0x8000 then script_value = memory.readbyte(script_address) end
            user_contest_calls:write(string.format(
                "%d,%04X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
                frame, address, object, memory.readbyte(0x0340 + object),
                memory.readbyte(0x0340), memory.readbyte(0x005A),
                memory.readbyte(0x0056), memory.readbyte(0x005B),
                memory.readbyte(0x0048), memory.readbyte(0x0670 + object),
                memory.readbyte(0x0680 + object), memory.readbyte(0x0370 + object),
                memory.readbyte(0x0410 + object), memory.readbyte(0x0370),
                memory.readbyte(0x0410), script_low, script_high, script_value,
                paired, paired < 0x10 and memory.readbyte(0x0340 + paired) or 0xFF))
        end
    end)
end
memory.registerexecute(0xA37D, 1, function(address, size, value)
    local frame = emu.framecount()
    if frame >= trace_start and frame <= trace_end and current_switch_bank() == 0 then
        local current = memory.readbyte(0x004B)
        local defender = current < 0x07 and 0x07 or 0x02
        if frame == inject_exceptional_reason_frame then
            memory.writebyte(0x0025, 0x00)
            memory.writebyte(0x0340, 0x01)
            memory.writebyte(0x005B, current)
            memory.writebyte(0x0048, current)
            memory.writebyte(0x0670 + current, 0x01)
            memory.writebyte(0x05B0 + current, 0x80)
            memory.writebyte(0x05C0 + current, 0x00)
            memory.writebyte(0x05B0 + defender, 0x80)
            memory.writebyte(0x05C0 + defender, 0x00)
            memory.writebyte(0x0340 + defender, 0x22)
            memory.writebyte(0x0350 + current, 0x04)
            memory.writebyte(0x0350 + defender, 0x00)
        end
        exceptional_calls:write(string.format("%d,%02X,%02X,%02X,%02X,%02X%02X,%02X,%02X,%02X,%02X%02X,%02X\n",
            frame, current, memory.readbyte(0x0025), memory.readbyte(0x0340),
            memory.readbyte(0x0670 + current), memory.readbyte(0x05C0 + current),
            memory.readbyte(0x05B0 + current), memory.readbyte(0x0350 + current),
            defender, memory.readbyte(0x0340 + defender), memory.readbyte(0x05C0 + defender),
            memory.readbyte(0x05B0 + defender), memory.readbyte(0x0350 + defender)))
    end
end)

local capture_frames = {
    [2080]=true,[2104]=true,[2106]=true,[2108]=true,[2110]=true,[2112]=true,[2114]=true,
    [2320]=true,[2340]=true,[2360]=true,[2380]=true,[2400]=true,[2420]=true,[2440]=true,
    [2460]=true,[2480]=true,[2500]=true,[2519]=true,[2520]=true,[2530]=true,[2531]=true,
    [2540]=true,[2550]=true,[2557]=true,[2560]=true,[2570]=true,[2580]=true,[2590]=true,
    [2600]=true,[2602]=true,[2606]=true,[2608]=true,[2614]=true,[2618]=true,[2620]=true,[2640]=true,
    [2644]=true,[2658]=true,[2660]=true,[2680]=true,[2684]=true,[2700]=true,[2723]=true,[2749]=true,[2760]=true,[2774]=true,
    [2770]=true,[2783]=true,[2788]=true,[2789]=true,[2790]=true,[2791]=true,[2792]=true,
    [2796]=true,[2798]=true,[2805]=true,[2807]=true,[2815]=true,[2816]=true,
    [2820]=true,[2830]=true,[2840]=true,[2860]=true,[2886]=true,[2914]=true,[2916]=true,
    [2929]=true,[2942]=true,[2944]=true,[3004]=true,[3324]=true,
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
    if score_audio_freeze_frame >= 0 and next_frame >= score_audio_freeze_frame then
        -- Audio-only score probe: retain the driver's already queued $18 and
        -- $1F/$22 streams while preventing a rebound, dribble, or later shot
        -- from requesting another effect. This instrumentation never feeds
        -- runtime assets directly; it only isolates the original APU output.
        memory.writebyte(0x0340, 0x0C)
        memory.writebyte(0x005B, 0xFF)
        memory.writebyte(0x0048, 0xFF)
        for slot = 0x02, 0x0B do
            memory.writebyte(0x0340 + slot, 0x37)
            memory.writebyte(0x0390 + slot, 0x00)
            memory.writebyte(0x03A0 + slot, 0x00)
            memory.writebyte(0x03E0 + slot, 0x00)
            memory.writebyte(0x03F0 + slot, 0x00)
        end
    end
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
    if next_frame >= move_start and next_frame <= move_end and move_direction ~= "none" then
        input[move_direction] = true
    end
    if next_frame == inject_user_steal_frame and inject_user_steal_button == "A" then
        input.A = true
    end
    if next_frame == pass_frame - 1 and user_position_frame < 0 and user_shot_depth >= 0 then
        -- Controlled full-path shooting probe.  Only the user carrier's court
        -- depth is changed before the real B-button edge; $AA75, $A504,
        -- $B189, $AE25 and $B377 remain unmodified and decide the outcome.
        memory.writebyte(0x03C0 + 0x02, user_shot_depth)
        if user_shot_x >= 0 then
            memory.writebyte(0x0360 + 0x02, math.floor(user_shot_x / 0x100))
            memory.writebyte(0x0370 + 0x02, user_shot_x % 0x100)
        end
    end
    if next_frame == user_position_frame and user_shot_depth >= 0 and user_shot_x >= 0 then
        -- Dynamic dunk probe: preload the carrier several live frames before
        -- B so the original run vector and near-basket eligibility chain can
        -- execute naturally instead of teleporting on the shot edge.
        memory.writebyte(0x0360 + 0x02, math.floor(user_shot_x / 0x100))
        memory.writebyte(0x0370 + 0x02, user_shot_x % 0x100)
        memory.writebyte(0x03C0 + 0x02, user_shot_depth)
    end
    if next_frame == inject_inbound_rule_frame and inject_inbound_rule_case ~= 0 then
        -- Controlled proofs for $A1CC reasons $13/$14, $9583 reason $15,
        -- and $95E0-$9635 reason $16.
        -- Object $02 is the user carrier; the ball remains safely inside the
        -- sloped $95E0 boundary so only the requested possession rule fires.
        local player = 0x02
        memory.writebyte(0x0340, 0x01)
        memory.writebyte(0x0340 + player, 0x02)
        memory.writebyte(0x005B, player)
        memory.writebyte(0x0048, player)
        memory.writebyte(0x004D, player)
        memory.writebyte(0x0050, 0x40)
        memory.writebyte(0x06B2, 0x40)
        memory.writebyte(0x0056, 0x00)
        memory.writebyte(0x0360, 0x00)
        memory.writebyte(0x0370, 0x80)
        memory.writebyte(0x03C0, 0x58)
        memory.writebyte(0x0360 + player, 0x00)
        memory.writebyte(0x0370 + player, 0x80)
        memory.writebyte(0x03C0 + player, 0x58)
        memory.writebyte(0x06B0, 0x00)
        memory.writebyte(0x06B1,
            inject_inbound_rule_case == 1 and 0x0A or
            (inject_inbound_rule_case == 2 and 0x18 or 0x00))
        memory.writebyte(0x06B3, inject_inbound_rule_case == 3 and 0x01 or 0x00)
        if inject_inbound_rule_case == 4 then
            memory.writebyte(0x0340, 0x07)
            memory.writebyte(0x005B, 0xFF)
            memory.writebyte(0x0048, 0xFF)
            memory.writebyte(0x0360, 0x00)
            memory.writebyte(0x0370, 0x80)
            memory.writebyte(0x03C0, 0x10)
            memory.writebyte(0x0410, 0x00)
            memory.writebyte(0x0390, 0x00)
            memory.writebyte(0x03E0, 0x00)
            memory.writebyte(0x0430, 0x00)
            memory.writebyte(0x0440, 0x00)
        end
    end
    if next_frame + 1 == inject_exceptional_reason_frame then
        -- Put the user carrier into the dispatcher one frame before the
        -- $A37D execution hook installs its branch-specific comparison data.
        memory.writebyte(0x0340, 0x01)
        memory.writebyte(0x0340 + 0x02, 0x02)
        memory.writebyte(0x005B, 0x02)
        memory.writebyte(0x0048, 0x02)
        memory.writebyte(0x0050, 0x40)
    end
    if next_frame == inject_block_frame then
        -- Controlled branch proof for bank-0 $8B12->$A6C3.  The natural
        -- frame-2606 contest already has the shifted X coordinates aligned,
        -- but the shot is too high.  Put only the ball's integer X/height
        -- inside the traced 4x4 boxes so $8B27's ownership write is observed.
        local defender = 0x07
        local shifted_x = memory.readbyte(0x0360 + defender) * 0x100 +
            memory.readbyte(0x0370 + defender) - 0x06
        if shifted_x < 0 then shifted_x = shifted_x + 0x10000 end
        memory.writebyte(0x0360, math.floor(shifted_x / 0x100))
        memory.writebyte(0x0370, shifted_x % 0x100)
        memory.writebyte(0x0410, (memory.readbyte(0x0410 + defender) + 0x08) % 0x100)
        memory.writebyte(0x0420, memory.readbyte(0x0420 + defender))
    end
    if next_frame == inject_user_block_frame then
        -- Controlled user `$A638->$A6C3` proof. Preserve the natural state
        -- `$11` jump and airborne CPU shot, changing only ball X/height so
        -- the original shifted 4x4 boxes accept contact and request `$20`.
        local defender = 0x02
        local shifted_x = memory.readbyte(0x0360 + defender) * 0x100 +
            memory.readbyte(0x0370 + defender) + 0x06
        if shifted_x >= 0x10000 then shifted_x = shifted_x - 0x10000 end
        memory.writebyte(0x0360, math.floor(shifted_x / 0x100))
        memory.writebyte(0x0370, shifted_x % 0x100)
        memory.writebyte(0x0410, (memory.readbyte(0x0410 + defender) + 0x08) % 0x100)
        memory.writebyte(0x0420, memory.readbyte(0x0420 + defender))
        memory.writebyte(0x0056, 0x00)
        memory.writebyte(0x005A, 0x00)
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
        local owner = 0x07
        memory.writebyte(0x0340,
            inject_contact_ball_state >= 0 and inject_contact_ball_state or 0x01)
        memory.writebyte(0x005B, owner)
        memory.writebyte(0x0048, owner)
        memory.writebyte(0x0050, 0x08)
        memory.writebyte(0x0068,
            inject_contact_limit >= 0 and inject_contact_limit or player)
        if inject_contact_level >= 0 then
            memory.writebyte(0x07E8, inject_contact_level)
            if inject_contact_level == 0 then memory.writebyte(0x006C, 0x40)
            elseif inject_contact_level == 4 then memory.writebyte(0x006C, 0x28)
            elseif inject_contact_level == 8 then memory.writebyte(0x006C, 0x1A)
            end
        end
        if inject_contact_phase >= 0 then
            memory.writebyte(0x001A, inject_contact_phase)
        end
        memory.writebyte(0x0360, 0x00)
        memory.writebyte(0x0370, 0x80)
        memory.writebyte(0x03C0, 0x58)
        memory.writebyte(0x0410, 0x10)
        memory.writebyte(0x0360 + owner, 0x00)
        memory.writebyte(0x0370 + owner, 0x80)
        memory.writebyte(0x03C0 + owner, 0x58)
        memory.writebyte(0x0410 + owner, 0x10)
        memory.writebyte(0x0340 + owner, 0x3B)
        memory.writebyte(0x0340 + player, 0x3B)
        memory.writebyte(0x0350 + player, memory.readbyte(0x0350 + owner))
        memory.writebyte(0x0360 + player, 0x00)
        memory.writebyte(0x0370 + player, 0x80)
        memory.writebyte(0x03C0 + player, 0x58)
        memory.writebyte(0x0410 + player, 0x10)
        memory.writebyte(0x0580 + player,
            inject_contact_pair == "wrong" and (owner + 1) or owner)
        memory.writebyte(0x0690 + player, 0x01)
        memory.writebyte(0x06A0 + player, 0x00)
        if inject_contact_clock_gate >= 0 then
            memory.writebyte(0x0025, inject_contact_clock_gate)
        end
    end
    if next_frame == inject_user_free_throw_frame then
        -- Controlled entry immediately before the original `$872F` ready
        -- handler. All subsequent readiness, aim oscillation, user input,
        -- height-script release, make/miss, and continuation code is original.
        memory.writebyte(0x0340, 0x00)
        memory.writebyte(0x005B, 0x02)
        memory.writebyte(0x0048, 0x02)
        memory.writebyte(0x0056, 0x00)
        memory.writebyte(0x0050, 0x00)
        memory.writebyte(0x0066, 0x00)
        memory.writebyte(0x0067, 0x00)
        memory.writebyte(0x002C, 0x00)
        for slot = 0x02, 0x0B do
            memory.writebyte(0x0340 + slot, 0x44)
            memory.writebyte(0x0690 + slot, (slot - 0x02) % 5)
        end
        memory.writebyte(0x0340 + 0x02, 0x45)
        memory.writebyte(0x0360 + 0x02, 0x00)
        memory.writebyte(0x0370 + 0x02, 0x92)
        memory.writebyte(0x03C0 + 0x02, 0x58)
        memory.writebyte(0x0410 + 0x02, 0x10)
    end
    if next_frame == inject_cpu_free_throw_frame then
        -- Controlled entry immediately before the original `$87F7->$8832`
        -- CPU free-throw decision handler.
        local cpu_shooter = 0x07
        memory.writebyte(0x0340, 0x00)
        memory.writebyte(0x005B, cpu_shooter)
        memory.writebyte(0x0048, cpu_shooter)
        memory.writebyte(0x0056, inject_cpu_free_throw_gate >= 0 and inject_cpu_free_throw_gate or 0x00)
        memory.writebyte(0x0050, 0x08)
        memory.writebyte(0x0066, 0x00)
        memory.writebyte(0x0067, inject_cpu_free_throw_timer >= 0 and inject_cpu_free_throw_timer or 0x00)
        memory.writebyte(0x002C, 0x00)
        if inject_cpu_free_throw_level >= 0 then
            memory.writebyte(0x07E8, inject_cpu_free_throw_level)
        end
        if inject_cpu_free_throw_phase >= 0 then
            memory.writebyte(0x001A, inject_cpu_free_throw_phase)
        end
        for slot = 0x02, 0x0B do
            memory.writebyte(0x0340 + slot, 0x44)
            memory.writebyte(0x0690 + slot, (slot - 0x02) % 5)
        end
        memory.writebyte(0x0340 + cpu_shooter, 0x46)
        memory.writebyte(0x0360 + cpu_shooter, 0x00)
        memory.writebyte(0x0370 + cpu_shooter, 0x6E)
        memory.writebyte(0x03C0 + cpu_shooter, 0x58)
        memory.writebyte(0x0410 + cpu_shooter, 0x10)
        if inject_cpu_free_throw_aim >= 0 then
            memory.writebyte(0x033C, inject_cpu_free_throw_aim)
        end
    end
    if next_frame == inject_user_steal_frame then
        local defender = 0x02
        local cpu_carrier = 0x07
        memory.writebyte(0x004B, defender)
        memory.writebyte(0x0340 + defender, 0x0F)
        memory.writebyte(0x0690 + defender, 0x00)
        memory.writebyte(0x0580 + defender, cpu_carrier)
        memory.writebyte(0x0580 + cpu_carrier, defender)
        memory.writebyte(0x0340 + cpu_carrier, inject_user_steal_paired_action >= 0 and inject_user_steal_paired_action or 0x25)
        memory.writebyte(0x0690 + cpu_carrier, 0x00)
        memory.writebyte(0x005B, inject_user_steal_same_player and defender or cpu_carrier)
        memory.writebyte(0x0048, cpu_carrier)
        memory.writebyte(0x0050, 0x08)
        memory.writebyte(0x001D, inject_user_steal_lock >= 0 and inject_user_steal_lock or 0x00)
        memory.writebyte(0x0056, inject_user_steal_gate >= 0 and inject_user_steal_gate or 0x00)
        memory.writebyte(0x0340, inject_user_steal_ball_state >= 0 and inject_user_steal_ball_state or 0x01)
        memory.writebyte(0x002C, 0x01)

        memory.writebyte(0x0360 + cpu_carrier, 0x00)
        memory.writebyte(0x0370 + cpu_carrier, 0x90)
        memory.writebyte(0x03C0 + cpu_carrier, 0x58)
        memory.writebyte(0x0410 + cpu_carrier, 0x10)

        memory.writebyte(0x0360, 0x00)
        memory.writebyte(0x0370, 0x90)
        memory.writebyte(0x03C0, 0x58)
        memory.writebyte(0x0410, 0x10)

        if inject_user_steal_collision == "hit" then
            memory.writebyte(0x0360 + defender, 0x00)
            memory.writebyte(0x0370 + defender, 0x90)
            memory.writebyte(0x03C0 + defender, 0x58)
            memory.writebyte(0x0410 + defender, 0x10)
        elseif inject_user_steal_collision == "boundary_in" then
            memory.writebyte(0x0360 + defender, 0x00)
            memory.writebyte(0x0370 + defender, 0x9A)
            memory.writebyte(0x03C0 + defender, 0x62)
            memory.writebyte(0x0410 + defender, 0x10)
        elseif inject_user_steal_collision == "boundary_out" then
            memory.writebyte(0x0360 + defender, 0x00)
            memory.writebyte(0x0370 + defender, 0x9B)
            memory.writebyte(0x03C0 + defender, 0x63)
            memory.writebyte(0x0410 + defender, 0x10)
        else
            memory.writebyte(0x0360 + defender, 0x00)
            memory.writebyte(0x0370 + defender, 0x50)
            memory.writebyte(0x03C0 + defender, 0x20)
            memory.writebyte(0x0410 + defender, 0x10)
        end

        if inject_user_steal_button == "A" then
            memory.writebyte(0x0680 + defender, 0x80)
        else
            memory.writebyte(0x0680 + defender, 0x00)
        end

        if inject_user_steal_foul then
            memory.writebyte(0x0025, 0x00)
            memory.writebyte(0x0350 + defender, 0x04)
            memory.writebyte(0x0350 + cpu_carrier, 0x04)
        else
            memory.writebyte(0x0025, 0x50)
            memory.writebyte(0x0350 + defender, 0x00)
            memory.writebyte(0x0350 + cpu_carrier, 0x04)
        end

        capture_frames[inject_user_steal_frame] = true
        capture_frames[inject_user_steal_frame + 1] = true
        capture_frames[inject_user_steal_frame + 2] = true
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
        memory.writebyte(0x005F, inject_basket_shot_kind)
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
level_contact:close()
score_calls:close()
cpu_decisions:close()
cpu_region2:close()
physics_calls:close()
control_calls:close()
block_calls:close()
user_contest_calls:close()
shot_kind_calls:close()
sfx_calls:close()
exceptional_calls:close()
shot_animation:close()
free_throw_calls:close()
local count_file = assert(io.open(join_path(capture_root, "tipoff-pc-counts.csv"), "w"))
count_file:write("phase,address,count\n")
for phase, phase_counts in pairs(counts) do
    for address, count in pairs(phase_counts) do
        count_file:write(string.format("%s,%04X,%d\n", phase, address, count))
    end
end
count_file:close()
local bank_count_file = assert(io.open(join_path(capture_root, "gameplay-bank-pc-counts.csv"), "w"))
bank_count_file:write("bank,address,count\n")
for bank, addresses in pairs(bank_counts) do
    for address, count in pairs(addresses) do
        bank_count_file:write(string.format("%d,%04X,%d\n", bank, address, count))
    end
end
bank_count_file:close()
emu.exit()
