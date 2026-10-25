// Pocket voice recorder enclosure, after the MVP concept
// (docs/reference/concept-mvp.png). Parametric OpenSCAD; sizes in millimetres.
//
// Render a part:   openscad -D 'part="back"' -o back.stl recorder.scad
// Parts: assembly (preview only), back, front, button, clip (optional).
// Each printable part comes out already in its print orientation.
//
// Values marked MEASURE are taken from listings or guessed, not from parts in
// hand. Measure yours with calipers before printing; everything else follows
// from them. See docs/DESIGN.md for the build.
//
// Axes: x across the face, y through it (front = -y, where the screen is),
// z along the length (z = 0 is the USB-C end).

part = "assembly";
with_clip = false;   // the concept has no clip; true adds screw holes for one

$fn = 48;

// --- Bought parts (MEASURE) -----------------------------------------------
// Seeed XIAO ESP32-S3 with the Sense expansion board, camera unplugged.
xiao_l = 21.0;        // along the body, USB-C end first (listing: 21 x 17.8)
xiao_w = 17.8;
xiao_stack = 10.0;    // MEASURE: board + expansion board + USB-C, no camera
usb_w = 9.4;          // opening for the USB-C receptacle (8.9 x 3.2 plus play)
usb_t = 3.8;
usb_offset_t = 0.0;   // MEASURE: USB-C centre from the cavity centre, + = toward the back
mic_from_usb = 10.0;  // MEASURE: mic port distance from the USB-C end of the board
mic_x = 0.0;          // MEASURE

// Good Display GDEM0097T61, 0.97" e-ink, portrait. Outline and active area
// from the vendor page; where the active area sits in the outline is a guess.
epd_l = 30.0;
epd_w = 14.15;
epd_t = 1.0;
epd_active_l = 22.26;
epd_active_w = 10.65;
epd_active_shift = 1.5;  // MEASURE: active-area centre offset away from the FPC end
adapter_t = 3.5;      // MEASURE: DESPI-C02-CV0097, taped behind the panel

// Single-cell LiPo pouch with protection board, e.g. size code 601535.
lipo_l = 35.0;
lipo_w = 15.0;
lipo_t = 6.0;

// 6 x 6 mm tactile switch, mounted on its side so the button is on the edge.
switch_w = 6.0;
switch_total = 5.0;   // body + actuator

// 3 mm orange LED, M2 x 5 flat-head screws (self-tapping into plastic).
led_d = 3.0;
screw_d = 2.0;

// --- Printer fit --------------------------------------------------------------
clear = 0.4;          // gap around loose parts
fit = 0.15;           // per-side gap where printed parts meet
wall = 1.4;
end_wall = 1.2;       // USB-C end: thin so the plug seats fully
corner_r = 3.5;       // corners seen from the front
edge_r = 1.5;         // the long edges
inner_r = 0.6;        // inside corners: tight, so the board's square corners fit

// --- Derived ----------------------------------------------------------------
in_w = xiao_w + 2 * clear;
in_t = max(xiao_stack, lipo_t, epd_t + adapter_t, switch_w) + 2 * clear;
out_w = in_w + 2 * wall;
out_t = in_t + 2 * wall;
split_y = -out_t / 2 + wall;  // the front lid is everything in front of this

btn_band = switch_w + 1.8;  // full-width band for the side button: switch plus two 0.8 ribs
disp_zone = epd_l + 3;
top_band = 8;         // LED and the two screws

z_board = end_wall;
z_batt = z_board + xiao_l + clear;
z_btn = z_batt + lipo_l + 2;
z_disp = z_btn + btn_band;
z_top = z_disp + disp_zone;
out_l = z_top + top_band + wall;

window_l = epd_active_l + 0.6;
window_w = epd_active_w + 0.6;
window_z = z_disp + 1.5 + epd_l / 2 + epd_active_shift;  // 1.5 = room for the flex-cable fold
led_z = z_top + top_band / 2;
boss_d = 4.6;
// Four corner screws, like the concept: two above the screen, two just above
// the board on short posts in front of the battery.
screw_low_z = z_batt + boss_d / 2 + 0.5;
screws = [[-5.5, led_z], [5.5, led_z], [-(in_w / 2 - boss_d / 2), screw_low_z], [in_w / 2 - boss_d / 2, screw_low_z]];
post_back_y = in_t / 2 - lipo_t - 0.6;  // lower posts stop in front of the battery
wedge_w = (in_w - lipo_w) / 2 - 0.2;    // the wedges under them fill the space beside the battery
rim_z = screw_low_z + boss_d / 2 + 0.5;  // the lid's alignment rim starts above the lower posts
lip_h = 1.2;          // the lid's alignment rim, reaching into the back
lip_w = 1.0;

btn_hole_t = 3.0;     // stadium hole in the right side, through-thickness size
btn_hole_l = 5.6;
btn_z = z_btn + btn_band / 2;
btn_x0 = in_w / 2 - fit - 0.8;    // inner face of the button flange, where the switch presses
sw_x0 = btn_x0 - switch_total;    // back of the switch, against the cradle

assert(lipo_w + clear <= in_w, "battery is wider than the inside");
assert(wedge_w >= 0.8, "battery too wide to leave room for the screw-post wedges");
assert(lipo_t + clear <= in_t, "battery is thicker than the inside");
assert(post_back_y - (split_y + wall) > 2.5, "no room for the lower screw posts in front of the battery");

// --- Shapes -------------------------------------------------------------------

// Box with rounded corners seen from the front (rc) and rounded long edges (re).
module rbox(w, t, l, rc, re) {
  hull()
    for (sx = [-1, 1], sz = [0, 1], sy = [-1, 1])
      translate([sx * (w / 2 - rc), sy * (t / 2 - re), rc + sz * (l - 2 * rc)])
        rotate([90, 0, 0]) rotate_extrude($fn = 32) translate([rc - re, 0]) circle(r = re, $fn = 16);
}

// Prism through y with rounded front-view corners.
module rprism(w, t, l, r) {
  hull()
    for (sx = [-1, 1], sz = [0, 1])
      translate([sx * (w / 2 - r), 0, r + sz * (l - 2 * r)]) rotate([90, 0, 0]) cylinder(r = r, h = t, center = true);
}

module stadium(a, b) {  // a across, b along z, axis along x
  hull() for (s = [-1, 1]) translate([0, 0, s * (b - a) / 2]) rotate([0, 90, 0]) cylinder(d = a, h = 10, center = true);
}

module shell() {
  difference() {
    rbox(out_w, out_t, out_l, corner_r, edge_r);
    translate([0, 0, end_wall]) rprism(in_w, in_t, out_l - end_wall - wall, inner_r);
  }
}

module front_half() { translate([-50, -50, -1]) cube([100, 50 + split_y, 200]); }

// --- Back: holds everything; printed face-down on its back ------------------

module back() {
  difference() {
    union() {
      difference() {
        shell();
        front_half();
      }
      // Top screw bosses, from the back up to the split.
      for (s = [0, 1]) translate([screws[s][0], 0, screws[s][1]])
        rotate([-90, 0, 0]) translate([0, 0, split_y]) cylinder(d = boss_d, h = in_t / 2 - split_y + wall / 2);
      // Lower posts hang off the side walls in front of the battery. A 45 degree
      // wedge against the wall carries the outer part when printing; the inner
      // ~3 mm of each post's underside is a short overhang. The battery sits
      // between the wedges.
      for (s = [2, 3]) let(x = screws[s][0], z = screws[s][1], wx = x > 0 ? in_w / 2 - wedge_w : -in_w / 2) {
        translate([x, split_y, z]) rotate([-90, 0, 0]) cylinder(d = boss_d, h = post_back_y - split_y);
        hull() {
          translate([wx, split_y, z - boss_d / 2]) cube([wedge_w, post_back_y - split_y, boss_d]);
          translate([x > 0 ? in_w / 2 - 0.01 : -in_w / 2, split_y, z - boss_d / 2]) cube([0.01, post_back_y - split_y + wedge_w, boss_d]);
        }
      }
      // Switch cradle: a wall the switch backs onto, and ribs either side.
      translate([sw_x0 - 1.6, split_y + lip_h + 0.3, z_btn]) cube([1.6, out_t / 2 - split_y - lip_h - 0.3, btn_band]);
      for (z = [z_btn, z_btn + btn_band - 0.8])
        translate([sw_x0 - 1.6, split_y + lip_h + 0.3, z]) cube([switch_total + 1.6 - fit, out_t / 2 - split_y - lip_h - 0.3, 0.8]);  // stop short of the button flange
      // Low ribs that keep the battery in its bay.
      for (z = [z_batt + 0.2, z_btn - 1.0])
        translate([-in_w / 2, in_t / 2 - 1.5, z]) cube([in_w, 1.5, 0.8]);
    }
    // USB-C port in the end.
    translate([0, usb_offset_t, -1]) hull()
      for (s = [-1, 1]) translate([s * (usb_w - usb_t) / 2, 0, 0]) cylinder(d = usb_t, h = end_wall + 2);
    // Mic holes in the back, over the mic.
    for (dx = [-1.6, 0, 1.6])
      translate([mic_x + dx, in_t / 2 - 0.5, end_wall + clear + mic_from_usb]) rotate([-90, 0, 0]) cylinder(d = 1.0, h = wall + 1);
    // Side button hole.
    translate([in_w / 2 + wall / 2, 0, btn_z]) stadium(btn_hole_t + 2 * fit, btn_hole_l + 2 * fit);
    // Screw pilot holes; the top two go through the back too when a clip screws on there.
    for (s = [0 : 3]) translate([screws[s][0], 0, screws[s][1]]) rotate([-90, 0, 0])
      translate([0, 0, split_y - 1]) cylinder(d = 1.7, h = (s < 2 && with_clip) ? out_t : 6);
  }
}

// --- Front: the lid with the screen window ----------------------------------

module front() {
  difference() {
    union() {
      intersection() {
        shell();
        front_half();
      }
      // Rim that drops into the back for alignment, from above the lower screw
      // posts up (the board fills the full width below them).
      difference() {
        translate([0, split_y + lip_h / 2, rim_z]) rprism(in_w - 2 * fit, lip_h, out_l - rim_z - wall - fit, inner_r);
        translate([0, split_y + lip_h / 2, rim_z - 1])
          rprism(in_w - 2 * fit - 2 * lip_w, lip_h + 1, out_l - rim_z - wall - fit - lip_w + 1, inner_r);
        for (s = screws) translate([s[0], 0, s[1]]) rotate([-90, 0, 0]) cylinder(d = boss_d + 0.6, h = 50, center = true);
      }
    }
    // Screen window, chamfered outward, and a shallow pocket that locates the panel.
    translate([0, split_y, window_z]) hull() {
      cube([window_w, 0.01, window_l], center = true);
      translate([0, -wall - 0.01, 0]) cube([window_w + 1.0, 0.01, window_l + 1.0], center = true);
    }
    translate([-(epd_w + 0.3) / 2, split_y - 0.4, z_disp + 1.5]) cube([epd_w + 0.3, 1, epd_l + 0.3]);
    // LED: small hole to the front, seat for the dome behind it.
    translate([0, split_y, led_z]) rotate([90, 0, 0]) cylinder(d = 2.0, h = wall + 1);
    translate([0, split_y + 0.01, led_z]) rotate([90, 0, 0]) cylinder(d = led_d + 0.3, h = 0.8);
    // Countersunk screw holes.
    for (s = screws) translate([s[0], split_y, s[1]]) rotate([90, 0, 0]) {
      translate([0, 0, -lip_h - 1]) cylinder(d = screw_d + 0.3, h = wall + lip_h + 2);
      translate([0, 0, wall - 1.0]) cylinder(d1 = screw_d + 0.3, d2 = screw_d * 2 + 0.2, h = 1.01);
    }
  }
}

// --- Button: pill on the right edge, flange keeps it inside -----------------

module button() {
  // Built along +x: flange against the switch, head through the wall.
  hull() for (s = [-1, 1]) translate([0, 0, s * (btn_hole_l - 2 - btn_hole_t + 2) / 2])
    rotate([0, 90, 0]) cylinder(d = btn_hole_t + 2, h = 0.8);
  hull() for (s = [-1, 1]) translate([0, 0, s * (btn_hole_l - btn_hole_t) / 2])
    rotate([0, 90, 0]) cylinder(d = btn_hole_t - 0.2, h = 0.8 + wall + 0.8);
}

// --- Clip (optional): screws onto the back with the lid screws' bosses -------

module clip() {
  root_l = 9;
  len = 48;
  gap = 0.8;   // room for the pocket fabric
  t = 1.6;
  difference() {
    union() {
      // Root block screwed to the back; the beam stands off it by gap.
      translate([-(2 * screws[1][0] + 6) / 2, 0, -root_l / 2]) cube([2 * screws[1][0] + 6, gap + t, root_l]);
      translate([-3, gap, -len]) cube([6, t, len]);
      // Bump that presses on the back (0.3 mm preload).
      translate([0, gap, -len + 3]) scale([3, gap + 0.3, 3]) sphere(1);
    }
    for (s = [0, 1]) translate([screws[s][0], -1, 0]) rotate([-90, 0, 0]) cylinder(d = screw_d + 0.3, h = gap + t + 2);
  }
}

// --- Output --------------------------------------------------------------------

module assembly() {
  color("WhiteSmoke") back();
  color("WhiteSmoke") front();
  color("Silver") translate([btn_x0, 0, btn_z]) button();
  // Stand-ins for the bought parts, to check fit.
  color("DarkSlateGray") translate([-xiao_w / 2, -xiao_stack / 2, z_board]) cube([xiao_w, xiao_stack, xiao_l]);
  color("Silver") translate([-lipo_w / 2, in_t / 2 - lipo_t, z_batt + 1]) cube([lipo_w, lipo_t, lipo_l]);
  color("Gainsboro") translate([-epd_w / 2, split_y + 0.02, z_disp + 1.5]) cube([epd_w, epd_t, epd_l]);
  color("DimGray") translate([-epd_w / 2, split_y + epd_t, z_disp + 1.5]) cube([epd_w, adapter_t, epd_l]);
  color("OrangeRed") translate([0, split_y + 0.5, led_z]) rotate([-90, 0, 0]) cylinder(d = led_d, h = 5);
  if (with_clip) color("WhiteSmoke") translate([0, out_t / 2, led_z]) clip();
}

if (part == "assembly") assembly();
if (part == "back") translate([0, 0, out_t / 2]) rotate([-90, 0, 0]) back();  // back face down
if (part == "front") translate([0, 0, out_t / 2]) rotate([90, 0, 0]) front();  // outer face down
if (part == "button") rotate([0, -90, 0]) button();                            // flange down
if (part == "clip") translate([0, 0, 2.4]) rotate([-90, 0, 0]) clip();                  // outer face down

echo(str("Recorder: ", out_w, " x ", out_t, " x ", out_l, " mm"));
