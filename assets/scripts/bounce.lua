-- bounce.lua
-- Makes an entity bounce up and down.
-- Attach to any entity with a Position component.

local speed = 2.0
local height = 3.0
local base_y = 3.0
local time_acc = 0.0

function on_init(entity_id)
    local pos = engine.get_position(entity_id)
    if pos then
        base_y = pos.y
    end
    engine.log("bounce.lua: init for entity " .. entity_id .. " base_y=" .. base_y)
end

function on_update(entity_id, dt)
    time_acc = time_acc + dt
    local pos = engine.get_position(entity_id)
    if pos then
        pos.y = base_y + math.abs(math.sin(time_acc * speed)) * height
    end
end
