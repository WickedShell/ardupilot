#include "mode.h"
#include "Plane.h"

bool ModeRTL::_enter()
{
  
    plane.throttle_allows_nudging = true;
    plane.auto_throttle_mode = true;
    plane.auto_navigation_mode = true;
    plane.prev_WP_loc = plane.current_loc;
    
    plane.last_low_altitude = plane.current_loc.alt;
    plane.low_altitude_count = 0;
    plane.low_airspeed_count = 0;
    plane.RTL_altitude_hold = 0;     
    plane.RTL_altitude = plane.get_RTL_altitude();
    plane.last_altitude_check_ms = AP_HAL::millis();
    
    // RTL Alttude Hold
    if (plane.g2.rtl_alt_hold == 1 && plane.current_loc.alt > plane.RTL_altitude + 2500) {
        plane.do_RTL(plane.current_loc.alt);
        gcs().send_text(MAV_SEVERITY_INFO, "Altitude Hold - RTL");
        plane.RTL_altitude_hold = 1;
    }
    else {
        plane.do_RTL(plane.RTL_altitude);
    }
    
    //dev messaging
    //float current_altitude = plane.current_loc.alt - plane.home.alt;
    //float rtl_alt = plane.RTL_altitude - plane.home.alt;
    //float wp_alt = plane.next_WP_loc.alt - plane.home.alt;
    //gcs().send_text(MAV_SEVERITY_INFO, "Alt: %.1f RTL-Alt: %.1f WP-Alt %.1f" ,current_altitude, rtl_alt, wp_alt); 
    
    
    // Should we be in QRTL instead? (are we hovering or failing to transition close to the home or a Rally location)
    if (plane.g2.efi.get_synthetic_arspd() <= 3 && plane.current_loc.get_distance(plane.next_WP_loc) < 1000.0){                
        plane.set_mode(plane.mode_qrtl, ModeReason::UNKNOWN);
        gcs().send_text(MAV_SEVERITY_CRITICAL, "Auto Switch - QRTL");
    }
    
         
    return true;
}

void ModeRTL::update()
{
    plane.calc_nav_roll();
    plane.calc_nav_pitch();
    plane.calc_throttle();
    
    //RTL Loiter Radius
    uint16_t radius = abs(plane.g.rtl_radius);
    if (radius > 0) {
        plane.loiter.direction = (plane.g.rtl_radius < 0) ? -1 : 1;
    }

    plane.update_loiter(radius);
    
    
    // RTL Altitude Monitor     
    if (AP_HAL::millis() - plane.last_altitude_check_ms > 1000) {            
        plane.last_altitude_check_ms = AP_HAL::millis();
          
        // RTL Altitude Hold Monitor (set altitude to Home or Rally point altitude once we are close) 
        if (plane.RTL_altitude_hold == 1 && plane.current_loc.get_distance(plane.next_WP_loc) < 500.0) {
            plane.do_RTL(plane.RTL_altitude);
            plane.RTL_altitude_hold = 0;
        }
              
        // Are we below our RTL Altitude?
        if (plane.current_loc.alt < plane.RTL_altitude - 200) {
            
            //dev messaging
            //float current_altitude = plane.current_loc.alt - plane.home.alt;
            //float current_distance = plane.current_loc.get_distance(plane.next_WP_loc);
            //float low_alt_cnt = plane.low_altitude_count;
            //float rally_alt = plane.RTL_altitude - plane.home.alt;
            //gcs().send_text(MAV_SEVERITY_INFO, "Alt: %.2f Dist: %.2f LAC: %.2f RalAlt: %.2f" ,current_altitude, current_distance, low_alt_cnt, rally_alt);
                       
            // Are we decending?
            if (plane.current_loc.alt < plane.last_low_altitude - 300) {
                plane.last_low_altitude = plane.current_loc.alt;
                plane.low_altitude_count ++;      
                     
                // Have we continued to decend for a total of 15 meters / over at least 5 seconds?
                if (plane.low_altitude_count > 4) {
     
                    // Set mode to QRTL then fly torwards our rally or home location until we either get there or reach our critical battery failsafe.
                    plane.set_mode(plane.mode_qrtl, ModeReason::UNKNOWN);
                    gcs().send_text(MAV_SEVERITY_CRITICAL, "LOW ALTITUDE - QRTL");                    
                }
            }
        }
        
        else {     
            plane.low_altitude_count = 0;
            plane.last_low_altitude = plane.current_loc.alt;
        }
    }
    
    
    // RTL Airspeed Monitor
    if (AP_HAL::millis() - plane.last_airspeed_check_ms > 1000) {            
        plane.last_airspeed_check_ms = AP_HAL::millis();
        
        // Are we flying slower than 75% of minimum airspeed?
        float aspeed;
        if (plane.ahrs.airspeed_estimate(aspeed)) {
            if (aspeed < plane.aparm.airspeed_min * .75) {
                plane.low_airspeed_count ++;
                
                //dev messaging
                //float low_arspd_cnt = plane.low_airspeed_count;        
                //gcs().send_text(MAV_SEVERITY_INFO, "ARSPD: %.2f LASC: %.2f" ,aspeed, low_arspd_cnt);
                
                // Have we been flying slow for at least 15 seconds? (we are likley stuck in transition with an engine starting failure)
                if (plane.low_airspeed_count > 15) {
                    
                    // Set mode to QRTL then fly torwards our rally or home location until we either get there or reach our critical battery failsafe.
                    plane.set_mode(plane.mode_qrtl, ModeReason::UNKNOWN);
                    gcs().send_text(MAV_SEVERITY_CRITICAL, "LOW AIRSPEED - QRTL");
                }
            }
            
            // If we have adequate airspeed count back to zero.
            else{
                plane.low_airspeed_count --;
            
                if (plane.low_airspeed_count < 0) {
                    plane.low_airspeed_count = 0;
                }
            }               
        }
    }   
}

