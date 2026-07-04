-- enemy_heavy.lua
-- Slow but tanky enemy: high HP, high damage, slow movement
-- Attach to entity with Position, RigidBody, and Enemy components.

local chase_speed = 1.5
local patrol_speed = 0.8
local patrol_range = 2.0
local time_acc = 0.0
local start_x = 0.0
local start_z = 0.0

function on_init(entity_id)
    engine.log("enemy_heavy.lua: init for entity " .. entity_id)
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
        engine.set_rigid_body_velocity(entity_id, (target_x - pos.x) * 2.0, 0.0, (target_z - pos.z) * 2.0)
        return
    end

    local dx = player_pos.x - pos.x
    local dz = player_pos.z - pos.z
    local dist = math.sqrt(dx * dx + dz * dz)

    if dist < 0.1 then
        engine.set_rigid_body_velocity(entity_id, 0.0, 0.0, 0.0)
        return
    end

    -- Chase when in range
    if dist < 12.0 then
        local vx = (dx / dist) * chase_speed
        local vz = (dz / dist) * chase_speed
        engine.set_rigid_body_velocity(entity_id, vx, 0.0, vz)
    else
        local target_x = start_x + math.sin(time_acc * patrol_speed) * patrol_range
        local target_z = start_z + math.cos(time_acc * patrol_speed * 0.7) * patrol_range
        engine.set_rigid_body_velocity(entity_id, (target_x - pos.x) * 2.0, 0.0, (target_z - pos.z) * 2.0)
    end

    -- Attack: high damage, slow
    if dist < 3.0 then
        engine.damage_player(25.0 * dt)
    end
end
