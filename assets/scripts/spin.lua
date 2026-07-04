-- spin.lua
-- Rotates an entity continuously around the Y axis.
-- Attach to any entity with a Rotation component.

local spin_speed = 90.0  -- degrees per second

function on_init(entity_id)
    engine.log("spin.lua: init for entity " .. entity_id)
end

function on_update(entity_id, dt)
    local rot = engine.get_rotation(entity_id)
    if rot then
        rot.angle = rot.angle + spin_speed * dt
        if rot.angle > 360.0 then
            rot.angle = rot.angle - 360.0
        end
    end
end
