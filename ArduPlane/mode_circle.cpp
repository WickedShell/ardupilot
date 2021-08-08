#include "mode.h"
#include "Plane.h"

bool ModeCircle::_enter()
{
    // SuperVolo - throttle is not working properly in this mode. transition directly to RTL
    plane.set_mode(plane.mode_rtl, ModeReason::UNKNOWN);
    gcs().send_text(MAV_SEVERITY_WARNING, "Circle bump to RTL");
    
    // the altitude to circle at is taken from the current altitude
    plane.throttle_allows_nudging = false;
    plane.auto_throttle_mode = true;
    plane.auto_navigation_mode = true;
    plane.next_WP_loc.alt = plane.current_loc.alt;

    return true;
}

void ModeCircle::update()
{
    // we have no GPS installed and have lost radio contact
    // or we just want to fly around in a gentle circle w/o GPS,
    // holding altitude at the altitude we set when we
    // switched into the mode
    plane.nav_roll_cd  = plane.roll_limit_cd / 3;
    plane.update_load_factor();
    plane.calc_nav_pitch();
    plane.calc_throttle();
}

