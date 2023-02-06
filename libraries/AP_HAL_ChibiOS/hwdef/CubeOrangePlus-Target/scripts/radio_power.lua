-- Copyright 2022, Spektreworks Inc, all rights reserved

local arming_auth_id = arming:get_aux_auth_id()

arming:set_aux_auth_failed(arming_auth_id, "Failed to start the radio and ignition control script")

local PARAM_TABLE = 6 -- first byte of sha256sum of file name
-- add the parameter table, allocate 3 parameters for it
assert (param:add_table(PARAM_TABLE, "TARG_", 6), "could not add param table")
assert (param:add_param(PARAM_TABLE, 1, "POWR_RELAY", 3), "could not add param")
assert (param:add_param(PARAM_TABLE, 2, "IGN_RELAY", 4), "could not add param")
assert (param:add_param(PARAM_TABLE, 3, "DEBUG", 1), "could not add param")
assert (param:add_param(PARAM_TABLE, 4, "POWR_DELAY", 1), "could not add param")
assert (param:add_param(PARAM_TABLE, 5, "POWR_TIME", 60), "could not add param")
assert (param:add_param(PARAM_TABLE, 6, "POWR_MODE", 2), "could not add param")

function bind_param(name)    
   local p = Parameter()    
   assert(p:init(name), string.format('could not find %s parameter', name))    
   return p    
end 

local power_mode = bind_param ("TARG_POWR_MODE")
-- Radio power modes
--   1 - RC Control
--   2 - Mission/GCS control (just set state on start and ignore)

local power_relay = bind_param ("TARG_POWR_RELAY")
relay:on (power_relay:get ()) -- force the radio on by default

local ignition_relay = bind_param ("TARG_IGN_RELAY")
relay:off (ignition_relay:get ())

local debug = bind_param ("TARG_DEBUG")
local radio_power_delay = bind_param ("TARG_POWR_DELAY")
local radio_delay_unit = bind_param ("TARG_POWR_TIME")

bind_param = nil -- just free the function, cause we don't need it

function get_radio_debounce (name, rc_channel_number)
  local radio_switches = {}
  radio_switches.name = name
  radio_switches.rc = assert(rc:find_channel_for_option(rc_channel_number), "missing channel for " .. name)
  radio_switches.last_position = SWITCH_INVALID -- last position before moving to this state
  radio_switches.current_pos   = SWITCH_INVALID -- current position we moved to
  radio_switches.debouce_pos   = SWITCH_INVALID -- position we might be debouncing into
  radio_switches.debounce_start = 0 -- time we started this position at
  return radio_switches
end

local timer_rc    = get_radio_debounce ("Timer", 300)

local SWITCH_INVALID = -1
local SWITCH_LOW     = 0
local SWITCH_MIDDLE  = 1
local SWITCH_HIGH    = 2


local debug_switch_names = {}
debug_switch_names [SWITCH_INVALID] = "invalid"
debug_switch_names [SWITCH_LOW]     = "low"
debug_switch_names [SWITCH_MIDDLE]  = "middle"
debug_switch_names [SWITCH_HIGH]    = "high"

local SWITCH_DEBOUNCE_TIME = 300

function debounce_channel (channel)
  if not rc:has_valid_input() then
    channel.debounce_pos = SWITCH_INVALID -- clear the debounce
    return false
  end 

  local switch_pos = channel.rc:get_aux_switch_pos()

  -- nothing changed, clear any in progress debouncing
  if switch_pos == channel.current_pos then
    channel.debounce_pos = SWITCH_INVALID
    return false
  end

  local time_ms = millis()

  -- new position detected, (re) start debouncing
  if switch_pos ~= channel.debounce_pos then
    channel.debounce_pos = switch_pos
    channel.debounce_start = time_ms
    if debug:get() > 2 then
      gcs:send_text(7, channel.name .. " started debounce for " .. debug_switch_names[switch_pos]);
    end
    return false
  end

  assert (switch_pos == channel.debounce_pos)

  if (time_ms - channel.debounce_start):tofloat() > SWITCH_DEBOUNCE_TIME then
    channel.last_position = channel.current_pos
    channel.current_pos = switch_pos
    channel.debounce_pos = SWITCH_INVALID
    if debug:get() > 1 then
      gcs:send_text(7, channel.name .. " moved to " .. debug_switch_names[switch_pos]);
    end
    return true
  end

  -- waiting on debounce to finish, nothing changed
  return false
end

local radio_off_timer = 0
local radio_delay = nil
local radio_poweroff_start = nil
local radio_power_current_on = true

function update_radio_power ()
  if power_mode:get () ~= 1 then
    return
  end
  -- always debounce a channel
  local input_changed = debounce_channel (timer_rc)

  if input_changed then
    if timer_rc.current_pos == SWITCH_LOW then
      if radio_off_timer > 0 then
        radio_low_timer = millis()
        if debug:get() > 1 then
          gcs:send_text(7, "Starting a clearance timer")
        end
      end
    elseif timer_rc.current_pos == SWITCH_MIDDLE then
      if timer_rc.last_position == SWITCH_LOW and radio_off_timer > 0 then
        -- shut the radio power off, provide a delay to give us time to actually emit the text message
        gcs:send_text(5, "Radio will be powered off for " .. tostring (radio_off_timer) .. " seconds");
        radio_delay_off = millis()
      elseif timer_rc.last_position == SWITCH_HIGH then -- FIXME: what should we do here?
        -- clear the counter
       -- radio_off_timer = 0
       -- gcs:send_text(6, "Radio power off timer cleared");
      end
    elseif timer_rc.current_pos == SWITCH_HIGH and timer_rc.last_position == SWITCH_LOW then
      radio_off_timer = radio_off_timer + radio_delay_unit:get ();
      if debug:get () > 0 then
        gcs:send_text(6, "Radio timer now " .. tostring(radio_off_timer) .. " seconds");
      end
    else
      -- this is a bad transition currently do nothing
      -- radio_off_timer = 0
      -- radio_low_timer = nil
      if debug:get () > 3 then
        gcs:send_text(7, "Radio bad transition " .. debug_switch_names[timer_rc.last_position] .. " to " .. debug_switch_names[timer_rc.current_pos])
      end
    end
  end

  if timer_rc.current_pos ~= SWITCH_LOW then
    radio_low_timer = nil
  end

  if radio_delay_off ~= nil then -- there's a delayed shutoff pending, apply it
    radio_low_timer = nil -- clear any old clearance timers
    local current_time = millis ()
    if (current_time - radio_delay_off):tofloat() > radio_power_delay:get() * 1000.0 then
      radio_power_current_on = false
      if debug:get() > 0 then
        gcs:send_text (6, "Radio power toggled off")
      end
      radio_delay_off = nil
      radio_poweroff_start = current_time
    end
  elseif radio_poweroff_start ~= nil then -- we are off, check if we should be on
    if (millis() - radio_poweroff_start):tofloat() > radio_off_timer * 1000 then
      radio_power_current_on = true
      radio_poweroff_start = nil
      radio_off_timer = 0;
      if debug:get () > 0 then
        gcs:send_text (7, "Radio power restored")
      end
    end
  elseif radio_low_timer ~= nil then
    if (millis() - radio_low_timer):tofloat() > 5000 then
      radio_off_timer = 0
      radio_low_timer = nil
      if debug:get() > 0 then
        gcs:send_text(6, "Radio timer cleared");
      end
    end
  end

  if radio_power_current_on then
    relay:on (power_relay:get ())
  else
    relay:off (power_relay:get ())
  end
end

local ignition_rc = get_radio_debounce ("Ignition", 301)
local ignition_on = true
function update_ignition ()
  debounce_channel(ignition_rc)
  if arming:is_armed() and (not rc:has_valid_input() or ignition_rc.current_pos == SWITCH_HIGH) then
    relay:on (ignition_relay:get ())
    if not igntion_on then
      igntion_on = true
      gcs:send_text (6, "Ignition enabled")
    end
  else
    relay:off (ignition_relay:get ())
    if igntion_on then
      igntion_on = false
      gcs:send_text (6, "Ignition disabled")
    end
  end
end

function update()
  update_radio_power ()
  update_ignition ()

  return update, 20
end
 
arming:set_aux_auth_passed(arming_auth_id)
arming_auth_id = nil
return update()