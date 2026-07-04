-- enemy_chase.lua
-- Enemy AI: patrol when far, chase and damage player when close.
-- Attach to entity with Position, RigidBody, and Enemy components.
-- Requires engine.get_player_position(), engine.damage_player(amount), engine.set_rigid_body_velocity().

local patrol_speed = 2.0
local chase_speed = 5.0
local patrol_range = 4.0
local time_acc = 0.0
local start_x = 0.0
local start_z = 0.0

function on_init(entity_id)
    engine.log("enemy_chase.lua: init for entity " .. entity_id)
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
        -- No player — patrol (set velocity along patrol path)
        local target_x = start_x + math.sin(time_acc * patrol_speed) * patrol_range
        local target_z = start_z + math.cos(time_acc * patrol_speed * 0.7) * patrol_range
        local vx = (target_x - pos.x) * 3.0
        local vz = (target_z - pos.z) * 3.0
        engine.set_rigid_body_velocity(entity_id, vx, 0.0, vz)
        return
    end

    local dx = player_pos.x - pos.x
    local dz = player_pos.z - pos.z
    local dist_sq = dx * dx + dz * dz
    local dist = math.sqrt(dist_sq)

    if dist < 0.1 then
        engine.set_rigid_body_velocity(entity_id, 0.0, 0.0, 0.0)
        return
    end

    -- Chase range: set velocity toward player (only XZ)
    if dist < 15.0 then
        local vx = (dx / dist) * chase_speed
        local vz = (dz / dist) * chase_speed
        engine.set_rigid_body_velocity(entity_id, vx, 0.0, vz)
    else
        -- Patrol
        local target_x = start_x + math.sin(time_acc * patrol_speed) * patrol_range
        local target_z = start_z + math.cos(time_acc * patrol_speed * 0.7) * patrol_range
        local vx = (target_x - pos.x) * 3.0
        local vz = (target_z - pos.z) * 3.0
        engine.set_rigid_body_velocity(entity_id, vx, 0.0, vz)
    end

    -- Attack range: damage player
    if dist < 2.5 then
        engine.damage_player(15.0 * dt) -- 15 damage per second
    end
end
