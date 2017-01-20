/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *   AP_Landing.cpp - Landing logic handler for ArduPlane
 */

#include "AP_Landing.h"
#include <GCS_MAVLink/GCS.h>

// table of user settable parameters
const AP_Param::GroupInfo AP_Landing::var_info[] = {

    // @Param: SLOPE_RCALC
    // @DisplayName: Landing slope re-calc threshold
    // @Description: This parameter is used when using a rangefinder during landing for altitude correction from baro drift (RNGFND_LANDING=1) and the altitude correction indicates your altitude is lower than the intended slope path. This value is the threshold of the correction to re-calculate the landing approach slope. Set to zero to keep the original slope all the way down and any detected baro drift will be corrected by pitching/throttling up to snap back to resume the original slope path. Otherwise, when a rangefinder altitude correction exceeds this threshold it will trigger a slope re-calculate to give a shallower slope. This also smoothes out the approach when flying over objects such as trees. Recommend a value of 2m.
    // @Range: 0 5
    // @Units: meters
    // @Increment: 0.5
    // @User: Advanced
    AP_GROUPINFO("SLOPE_RCALC", 1, AP_Landing, slope_recalc_shallow_threshold, 2.0f),

    // @Param: ABORT_DEG
    // @DisplayName: Landing auto-abort slope threshold
    // @Description: This parameter is used when using a rangefinder during landing for altitude correction from baro drift (RNGFND_LANDING=1) and the altitude correction indicates your actual altitude is higher than the intended slope path. Normally it would pitch down steeply but that can result in a crash with high airspeed so this allows remembering the baro offset and self-abort the landing and come around for another landing with the correct baro offset applied for a perfect slope. An auto-abort go-around will only happen once, next attempt will not auto-abort again. This operation happens entirely automatically in AUTO mode. This value is the delta degrees threshold to trigger the go-around compared to the original slope. Example: if set to 5 deg and the mission planned slope is 15 deg then if the new slope is 21 then it will go-around. Set to 0 to disable. Requires LAND_SLOPE_RCALC > 0.
    // @Range: 0 90
    // @Units: degrees
    // @Increment: 0.1
    // @User: Advanced
    AP_GROUPINFO("ABORT_DEG", 2, AP_Landing, slope_recalc_steep_threshold_to_abort, 0),

    // @Param: PITCH_CD
    // @DisplayName: Landing Pitch
    // @Description: Used in autoland to give the minimum pitch in the final stage of landing (after the flare). This parameter can be used to ensure that the final landing attitude is appropriate for the type of undercarriage on the aircraft. Note that it is a minimum pitch only - the landing code will control pitch above this value to try to achieve the configured landing sink rate.
    // @Units: centi-Degrees
    // @User: Advanced
    AP_GROUPINFO("PITCH_CD", 3, AP_Landing, pitch_cd, 0),

    // @Param: FLARE_ALT
    // @DisplayName: Landing flare altitude
    // @Description: Altitude in autoland at which to lock heading and flare to the LAND_PITCH_CD pitch. Note that this option is secondary to LAND_FLARE_SEC. For a good landing it preferable that the flare is triggered by LAND_FLARE_SEC.
    // @Units: meters
    // @Increment: 0.1
    // @User: Advanced
    AP_GROUPINFO("FLARE_ALT", 4, AP_Landing, flare_alt, 3.0f),

    // @Param: FLARE_SEC
    // @DisplayName: Landing flare time
    // @Description: Vertical time before landing point at which to lock heading and flare with the motor stopped. This is vertical time, and is calculated based solely on the current height above the ground and the current descent rate.  Set to 0 if you only wish to flare based on altitude (see LAND_FLARE_ALT).
    // @Units: seconds
    // @Increment: 0.1
    // @User: Advanced
    AP_GROUPINFO("FLARE_SEC", 5, AP_Landing, flare_sec, 2.0f),

    // @Param: PF_ALT
    // @DisplayName: Landing pre-flare altitude
    // @Description: Altitude to trigger pre-flare flight stage where LAND_PF_ARSPD controls airspeed. The pre-flare flight stage trigger works just like LAND_FLARE_ALT but higher. Disabled when LAND_PF_ARSPD is 0.
    // @Units: meters
    // @Range: 0 30
    // @Increment: 0.1
    // @User: Advanced
    AP_GROUPINFO("PF_ALT", 6, AP_Landing, pre_flare_alt, 10.0f),

    // @Param: PF_SEC
    // @DisplayName: Landing pre-flare time
    // @Description: Vertical time to ground to trigger pre-flare flight stage where LAND_PF_ARSPD controls airspeed. This pre-flare flight stage trigger works just like LAND_FLARE_SEC but earlier. Disabled when LAND_PF_ARSPD is 0.
    // @Units: seconds
    // @Range: 0 10
    // @Increment: 0.1
    // @User: Advanced
    AP_GROUPINFO("PF_SEC", 7, AP_Landing, pre_flare_sec, 6.0f),

    // @Param: PF_ARSPD
    // @DisplayName: Landing pre-flare airspeed
    // @Description: Desired airspeed during pre-flare flight stage. This is useful to reduce airspeed just before the flare. Use 0 to disable.
    // @Units: m/s
    // @Range: 0 30
    // @Increment: 0.1
    // @User: Advanced
    AP_GROUPINFO("PF_ARSPD", 8, AP_Landing, pre_flare_airspeed, 0),

    // @Param: THR_SLEW
    // @DisplayName: Landing throttle slew rate
    // @Description: This parameter sets the slew rate for the throttle during auto landing. When this is zero the THR_SLEWRATE parameter is used during landing. The value is a percentage throttle change per second, so a value of 20 means to advance the throttle over 5 seconds on landing. Values below 50 are not recommended as it may cause a stall when airspeed is low and you can not throttle up fast enough.
    // @Units: percent
    // @Range: 0 127
    // @Increment: 1
    // @User: User
    AP_GROUPINFO("THR_SLEW", 9, AP_Landing, throttle_slewrate, 0),

    // @Param: DISARMDELAY
    // @DisplayName: Landing disarm delay
    // @Description: After a landing has completed using a LAND waypoint, automatically disarm after this many seconds have passed. Use 0 to not disarm.
    // @Units: seconds
    // @Increment: 1
    // @Range: 0 127
    // @User: Advanced
    AP_GROUPINFO("DISARMDELAY", 10, AP_Landing, disarm_delay, 20),

    // @Param: THEN_NEUTRL
    // @DisplayName: Set servos to neutral after landing
    // @Description: When enabled, after an autoland and auto-disarm via LAND_DISARMDELAY happens then set all servos to neutral. This is helpful when an aircraft has a rough landing upside down or a crazy angle causing the servos to strain.
    // @Values: 0:Disabled, 1:Servos to Neutral, 2:Servos to Zero PWM
    // @User: Advanced
    AP_GROUPINFO("THEN_NEUTRL", 11, AP_Landing, then_servos_neutral, 0),

    // @Param: ABORT_THR
    // @DisplayName: Landing abort using throttle
    // @Description: Allow a landing abort to trigger with a throttle > 95%
    // @Values: 0:Disabled, 1:Enabled
    // @User: Advanced
    AP_GROUPINFO("ABORT_THR", 12, AP_Landing, abort_throttle_enable, 0),

    // @Param: FLAP_PERCNT
    // @DisplayName: Landing flap percentage
    // @Description: The amount of flaps (as a percentage) to apply in the landing approach and flare of an automatic landing
    // @Range: 0 100
    // @Units: Percent
    // @User: Advanced
    AP_GROUPINFO("FLAP_PERCNT", 13, AP_Landing, flap_percent, 0),

    // @Param: TYPE
    // @DisplayName: Auto-landing type
    // @Description: Specifies the auto-landing type to use
    // @Values: 0:Standard Glide Slope
    // @User: Standard
    AP_GROUPINFO("TYPE",    14, AP_Landing, type, TYPE_STANDARD_GLIDE_SLOPE),

    // @Param: DS_V_FWD
    // @DisplayName: Deepstall forward velocity
    // @Description: The forward velocity of the aircraft while stalled
    // @Range: 0 20
    // @Units: m/s
    // @User: Advanced
    AP_GROUPINFO("DS_V_FWD", 15, AP_Landing, type_deepstall_forward_speed, 1),

    // @Param: DS_SLOPE_A
    // @DisplayName: Deepstall slope a
    // @Description: The a component of distance = a*wind + b
    // @User: Advanced
    AP_GROUPINFO("DS_SLOPE_A", 16, AP_Landing, type_deepstall_slope_a, 1),

    // @Param: DS_SLOPE_B
    // @DisplayName: Deepstall slope b
    // @Description: The a component of distance = a*wind + b
    // @User: Advanced
    AP_GROUPINFO("DS_SLOPE_B", 17, AP_Landing, type_deepstall_slope_b, 1),

    // @Param: DS_APP_EXT
    // @DisplayName: Deepstall approach extension
    // @Description: The forward velocity of the aircraft while stalled
    // @Range: 10 200
    // @Units: meters
    // @User: Advanced
    AP_GROUPINFO("DS_APP_EXT", 18, AP_Landing, type_deepstall_approach_extension, 50),

    // @Param: DS_V_DWN
    // @DisplayName: Deepstall veloicty down
    // @Description: The downward velocity of the aircraft while stalled
    // @Range: 0 20
    // @Units: m/s
    // @User: Advanced
    AP_GROUPINFO("DS_V_DWN", 19, AP_Landing, type_deepstall_down_speed, 2),

    // @Param: DS_SLEW_SPD
    // @DisplayName: Deepstall slew speed
    // @Description: The speed at which the elevator slews to deepstall
    // @Range: 0 2
    // @Units: seconds
    // @User: Advanced
    AP_GROUPINFO("DS_SLEW_SPD", 20, AP_Landing, type_deepstall_slew_speed, 0.5),

    // @Param: DS_ELEV_PWM
    // @DisplayName: Deepstall elevator PWM
    // @Description: The PWM value for the elevator at full deflection in deepstall
    // @Range: 900 2100
    // @Units: PWM
    // @User: Advanced
    AP_GROUPINFO("DS_ELEV_PWM", 21, AP_Landing, type_deepstall_elevator_pwm, 1500),

    // @Param: DS_ARSP_MAX
    // @DisplayName: Deepstall enabled airspeed
    // @Description: The maximum aispeed where the deepstall steering controller is allowed to have control
    // @Range: 5 20
    // @Units: m/s
    // @User: Advanced
    AP_GROUPINFO("DS_ARSP_MAX", 22, AP_Landing, type_deepstall_handoff_airspeed, 15.0),

    // @Param: DS_ARSP_MIN
    // @DisplayName: Deepstall minimum derating airspeed
    // @Description: Deepstall lowest airspeed where the deepstall controller isn't allowed full control
    // @Range: 5 20
    // @Units: m/s
    // @User: Advanced
    AP_GROUPINFO("DS_ARSP_MIN", 23, AP_Landing, type_deepstall_handoff_lower_limit_airspeed, 10.0),

    // @Param: DS_L1
    // @DisplayName: Deepstall L1 period
    // @Description: Deepstall L1 navigational controller period
    // @Range: 5 50
    // @Units: meters
    // @User: Advanced
    AP_GROUPINFO("DS_L1", 24, AP_Landing, type_deepstall_l1_period, 30.0),

    // @Param: DS_L1_I
    // @DisplayName: Deepstall L1 I gain
    // @Description: Deepstall L1 integratior gain
    // @Range: 0 1
    // @User: Advanced
    AP_GROUPINFO("DS_L1", 25, AP_Landing, type_deepstall_l1_period, 0),

    // @Param: DS_YAW_LIM
    // @DisplayName: Deepstall yaw rate limit
    // @Description: The yaw rate limit while navigating in deepstall
    // @Range: 0 90
    // @Units degrees per second
    // @User: Advanced
    AP_GROUPINFO("DS_YAW_LIM", 26, AP_Landing, type_deepstall_yaw_rate_limit, 10),

    // @Param: DS_L1_TCON
    // @DisplayName: Deepstall L1 time constant
    // @Description: Time constant for deepstall L1 control
    // @Range: 0 1
    // @Units seconds
    // @User: Advanced
    AP_GROUPINFO("DS_L1_TCON", 27, AP_Landing, type_deepstall_time_constant, 0.4),

    // @Group: DS_
    // @Path: ../libraries/PID/PID.cpp
    AP_SUBGROUPINFO(type_deepstall_PID, "DS_", 28, AP_Landing, PID),

    AP_GROUPEND
};

void AP_Landing::do_land(const AP_Mission::Mission_Command& cmd, const float relative_altitude)
{
    commanded_go_around = false;

    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        type_slope_do_land(cmd, relative_altitude);
        break;
    case TYPE_DEEPSTALL:
        type_deepstall_do_land(cmd, relative_altitude);
        break;
    default:
        // a incorrect type is handled in the verify_land
        break;
    }
}

/*
  update navigation for landing. Called when on landing approach or
  final flare
 */
bool AP_Landing::verify_land(const Location &prev_WP_loc, Location &next_WP_loc, const Location &current_loc,
        const float height, const float sink_rate, const float wp_proportion, const uint32_t last_flying_ms, const bool is_armed, const bool is_flying, const bool rangefinder_state_in_range)
{
    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        return type_slope_verify_land(prev_WP_loc, next_WP_loc, current_loc,
                height, sink_rate, wp_proportion, last_flying_ms, is_armed, is_flying, rangefinder_state_in_range);
    case TYPE_DEEPSTALL:
        return type_deepstall_verify_land(prev_WP_loc, next_WP_loc, current_loc,
                height, sink_rate, wp_proportion, last_flying_ms, is_armed, is_flying, rangefinder_state_in_range);
    default:
        // returning TRUE while executing verify_land() will increment the
        // mission index which in many cases will trigger an RTL for end-of-mission
        GCS_MAVLINK::send_statustext_all(MAV_SEVERITY_CRITICAL, "Landing configuration error, invalid LAND_TYPE");
        return true;
    }
}


bool AP_Landing::verify_abort_landing(const Location &prev_WP_loc, Location &next_WP_loc, const Location &current_loc,
    const int32_t auto_state_takeoff_altitude_rel_cm, bool &throttle_suppressed)
{
    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        type_slope_verify_abort_landing(prev_WP_loc, next_WP_loc, throttle_suppressed);
        break;
    case TYPE_DEEPSTALL:
        type_deepstall_verify_abort_landing(prev_WP_loc, next_WP_loc, throttle_suppressed);
        break;
    default:
        break;
    }

    // see if we have reached abort altitude
     if (adjusted_relative_altitude_cm_fn() > auto_state_takeoff_altitude_rel_cm) {
         next_WP_loc = current_loc;
         mission.stop();
         if (restart_landing_sequence()) {
             mission.resume();
         }
         // else we're in AUTO with a stopped mission and handle_auto_mode() will set RTL
     }

     // make sure to always return false so it leaves the mission index alone
     return false;
}

void AP_Landing::adjust_landing_slope_for_rangefinder_bump(AP_Vehicle::FixedWing::Rangefinder_State &rangefinder_state, Location &prev_WP_loc, Location &next_WP_loc, const Location &current_loc, const float wp_distance, int32_t &target_altitude_offset_cm)
{
    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        type_slope_adjust_landing_slope_for_rangefinder_bump(rangefinder_state, prev_WP_loc, next_WP_loc, current_loc, wp_distance, target_altitude_offset_cm);
        break;
    case TYPE_DEEPSTALL:
    default:
        break;
    }
}

// return true while the aircraft should be in a flaring state
// also enables ground steering, set the target_altitude to be the next_WP_loc
bool AP_Landing::is_flaring(void) const
{
    if (!in_progress) {
        return false;
    }

    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        return type_slope_is_flaring();
    case TYPE_DEEPSTALL:
    default:
        return false;
    }
}

// return true while the aircraft is performing a landing approach
// when true the vehicle will:
//   - disable ground steering
//   - call setup_landing_glide_slope() and adjust_landing_slope_for_rangefinder_bump()
//   - will be considered flying if sink rate > 0.2, and can trigger crash detection
bool AP_Landing::is_on_approach(void) const
{
    if (!in_progress) {
        return false;
    }

    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        return type_slope_is_on_approach();
    case TYPE_DEEPSTALL:
    default:
        return false;
    }
}

// return true while the aircraft is allowed to perform ground steering
bool AP_Landing::is_ground_steering_allowed(void) const
{
    if (!in_progress) {
        return true;
    }

    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        return type_slope_is_on_approach();
    case TYPE_DEEPSTALL:
        return false;
    default:
        return true;
    }
}
// return true when at the last stages of a land when an impact with the ground is expected soon
// when true is_flying knows that the vehicle was expecting to stop flying, possibly because of a hard impact
bool AP_Landing::is_expecting_impact(void) const
{
    if (!in_progress) {
        return false;
    }

    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        return type_slope_is_expecting_impact();
    case TYPE_DEEPSTALL:
    default:
        return false;
    }
}

bool AP_Landing::control_servos(void) {
    if (!in_progress) {
        return false;
    }

    switch (type) {
    case TYPE_DEEPSTALL:
        return type_deepstall_control_servos();
    case TYPE_STANDARD_GLIDE_SLOPE:
    default:
        return false;
    }
}

const DataFlash_Class::PID_Info& AP_Landing::get_pid_info(void) const
{
// FIXME: Figure out how to handle not knowing the PID
//    switch (type) {
//    case TYPE_DEEPSTALL:
        return type_deepstall_get_pid_info();
//    default:
//        return empty_pid;
//    }
}

/*
  a special glide slope calculation for the landing approach

  During the land approach use a linear glide slope to a point
  projected through the landing point. We don't use the landing point
  itself as that leads to discontinuities close to the landing point,
  which can lead to erratic pitch control
 */

void AP_Landing::setup_landing_glide_slope(const Location &prev_WP_loc, const Location &next_WP_loc, const Location &current_loc, int32_t &target_altitude_offset_cm)
{
    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        type_slope_setup_landing_glide_slope(prev_WP_loc, next_WP_loc, current_loc, target_altitude_offset_cm);
        break;
    case TYPE_DEEPSTALL:
    default:
        break;
    }
}

/*
     Restart a landing by first checking for a DO_LAND_START and
     jump there. Otherwise decrement waypoint so we would re-start
     from the top with same glide slope. Return true if successful.
 */
bool AP_Landing::restart_landing_sequence()
{
    if (mission.get_current_nav_cmd().id != MAV_CMD_NAV_LAND) {
        return false;
    }

    uint16_t do_land_start_index = mission.get_landing_sequence_start();
    uint16_t prev_cmd_with_wp_index = mission.get_prev_nav_cmd_with_wp_index();
    bool success = false;
    uint16_t current_index = mission.get_current_nav_index();
    AP_Mission::Mission_Command cmd;

    if (mission.read_cmd_from_storage(current_index+1,cmd) &&
            cmd.id == MAV_CMD_NAV_CONTINUE_AND_CHANGE_ALT &&
            (cmd.p1 == 0 || cmd.p1 == 1) &&
            mission.set_current_cmd(current_index+1))
    {
        // if the next immediate command is MAV_CMD_NAV_CONTINUE_AND_CHANGE_ALT to climb, do it
        GCS_MAVLINK::send_statustext_all(MAV_SEVERITY_NOTICE, "Restarted landing sequence. Climbing to %dm", cmd.content.location.alt/100);
        success =  true;
    }
    else if (do_land_start_index != 0 &&
            mission.set_current_cmd(do_land_start_index))
    {
        // look for a DO_LAND_START and use that index
        GCS_MAVLINK::send_statustext_all(MAV_SEVERITY_NOTICE, "Restarted landing via DO_LAND_START: %d",do_land_start_index);
        success =  true;
    }
    else if (prev_cmd_with_wp_index != AP_MISSION_CMD_INDEX_NONE &&
               mission.set_current_cmd(prev_cmd_with_wp_index))
    {
        // if a suitable navigation waypoint was just executed, one that contains lat/lng/alt, then
        // repeat that cmd to restart the landing from the top of approach to repeat intended glide slope
        GCS_MAVLINK::send_statustext_all(MAV_SEVERITY_NOTICE, "Restarted landing sequence at waypoint %d", prev_cmd_with_wp_index);
        success =  true;
    } else {
        GCS_MAVLINK::send_statustext_all(MAV_SEVERITY_WARNING, "Unable to restart landing sequence");
        success =  false;
    }

    if (success) {
        // exit landing stages if we're no longer executing NAV_LAND
        update_flight_stage_fn();
    }
    return success;
}

int32_t AP_Landing::constrain_roll(const int32_t desired_roll_cd, const int32_t level_roll_limit_cd)
{
    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        return type_slope_constrain_roll(desired_roll_cd, level_roll_limit_cd);
    case TYPE_DEEPSTALL:
    default:
        return desired_roll_cd;
    }
}

// returns true if landing provided a Location structure with the current target altitude
bool AP_Landing::get_target_altitude_location(Location &location)
{
    if (!in_progress) {
        return false;
    }

    switch (type) {
    case TYPE_DEEPSTALL:
        return type_deepstall_get_target_altitude_location(location);
    case TYPE_STANDARD_GLIDE_SLOPE:
    default:
        return false;
    }
}

/*
 * Determine how aligned heading_deg is with the wind. Return result
 * is 1.0 when perfectly aligned heading into wind, -1 when perfectly
 * aligned with-wind, and zero when perfect cross-wind. There is no
 * distinction between a left or right cross-wind. Wind speed is ignored
 */
float AP_Landing::wind_alignment(const float heading_deg)
{
    const Vector3f wind = ahrs.wind_estimate();
    const float wind_heading_rad = atan2f(-wind.y, -wind.x);
    return cosf(wind_heading_rad - radians(heading_deg));
}

/*
 * returns head-wind in m/s, 0 for tail-wind.
 */
float AP_Landing::head_wind(void)
{
    const float alignment = wind_alignment(ahrs.yaw_sensor*0.01f);

    if (alignment <= 0) {
        return 0;
    }

    return alignment * ahrs.wind_estimate().length();
}

/*
 * returns target airspeed in cm/s depending on flight stage
 */
int32_t AP_Landing::get_target_airspeed_cm(void)
{
    if (!in_progress) {
        // not landing, use regular cruise airspeed
        return aparm.airspeed_cruise_cm;
    }

    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        return type_slope_get_target_airspeed_cm();
    case TYPE_DEEPSTALL:
        return aparm.airspeed_cruise_cm;
    default:
        return SpdHgt_Controller->get_land_airspeed();
    }
}

/*
 * request a landing abort given the landing type
 * return true on success
 */
bool AP_Landing::request_go_around(void)
{
    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        return type_slope_request_go_around();
    case TYPE_DEEPSTALL:
        return type_deepstall_request_go_around();
    default:
        return false;
    }
}

void AP_Landing::handle_flight_stage_change(const bool _in_landing_stage)
{
    in_progress = _in_landing_stage;
    commanded_go_around = false;
}

/*
 * returns true when a landing is complete, usually used to disable throttle
 */
bool AP_Landing::is_complete(void) const
{
    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        return type_slope_is_complete();
    case TYPE_DEEPSTALL:
        return false;
    default:
        return true;
    }
}

/*
 * returns true when throttle should be suppressed while landing
 */
bool AP_Landing::is_throttle_suppressed(void) const
{
    if (!in_progress) {
        return false;
    }

    switch (type) {
    case TYPE_STANDARD_GLIDE_SLOPE:
        return type_slope_is_throttle_suppressed();
    case TYPE_DEEPSTALL:
        return type_deepstall_is_throttle_suppressed();
    default:
        return false;
    }
}

