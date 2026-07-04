-- pulse_light.lua
-- Makes a light pulse in intensity.
-- Attach to any entity with a Light component.

local base_intensity = 2.0
local pulse_amount = 1.5
local pulse_speed = 3.0
local time_acc = 0.0

function on_init(entity_id)
    local light = engine.get_light(entity_id)
    if light then
        base_intensity = light.intensity
    end
    engine.log("pulse_light.lua: init for entity " .. entity_id)
end

function on_update(entity_id, dt)
    time_acc = time_acc + dt
    local light = engine.get_light(entity_id)
    if light then
        light.intensity = base_intensity + math.sin(time_acc * pulse_speed) * pulse_amount
    end
end
