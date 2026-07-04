-- patrol.lua
-- Makes an entity move back and forth along the X axis.
-- Attach to any entity with a Position component.

local speed = 3.0
local range = 5.0
local time_acc = 0.0

function on_init(entity_id)
    engine.log("patrol.lua: init for entity " .. entity_id)
end

function on_update(entity_id, dt)
    time_acc = time_acc + dt
    local pos = engine.get_position(entity_id)
    if pos then
        pos.x = math.sin(time_acc * speed) * range
    end
end
