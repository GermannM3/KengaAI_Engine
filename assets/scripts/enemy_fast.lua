-- enemy_fast.lua
-- Fast but weak enemy: rushes player quickly, low HP
-- Attach to entity with Position, RigidBody, and Enemy components.

local chase_speed = 8.0
local patrol_speed = 3.0
local patrol_range = 3.0
local time_acc = 0.0
local start_x = 0.0
local start_z = 0.0

function on_init(entity_id)
    engine.log("enemy_fast.lua: init for entity " .. entity_id)
    local pos = engine.get_position(entity_id)
    if pos then
        start_x = pos.x
        start_z = pos.z
    end
end

function on_update(entity_id, dt)
    time_acc = time_acc + dt
    local pos = engine.get_position(entity_id)
    if not pos then return end

    local player_pos = engine.get_player_position()
    if not player_pos then
        local target_x = start_x + math.sin(time_acc * patrol_speed) * patrol_range
        local target_z = start_z + math.cos(time_acc * patrol_speed * 0.7) * patrol_range
        engine.set_rigid_body_velocity(entity_id, (target_x - pos.x) * 3.0, 0.0, (target_z - pos.z) * 3.0)
        return
    end

    local dx = player_pos.x - pos.x
    local dz = player_pos.z - pos.z
    local dist = math.sqrt(dx * dx + dz * dz)

    if dist < 0.1 then
        engine.set_rigid_body_velocity(entity_id, 0.0, 0.0, 0.0)
        return
    end

    -- Chase always when close enough
    if dist < 20.0 then
        local vx = (dx / dist) * chase_speed
        local vz = (dz / dist) * chase_speed
        engine.set_rigid_body_velocity(entity_id, vx, 0.0, vz)
    else
        local target_x = start_x + math.sin(time_acc * patrol_speed) * patrol_range
        local target_z = start_z + math.cos(time_acc * patrol_speed * 0.7) * patrol_range
        engine.set_rigid_body_velocity(entity_id, (target_x - pos.x) * 3.0, 0.0, (target_z - pos.z) * 3.0)
    end

    -- Attack: low damage but fast
    if dist < 2.0 then
        engine.damage_player(8.0 * dt)
    end
end
