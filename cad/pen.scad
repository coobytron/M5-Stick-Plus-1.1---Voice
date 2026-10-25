// Voice pen enclosure. Parametric OpenSCAD; every size is in millimetres.
//
// Render a part:   openscad -D 'part="body"' -o body.stl pen.scad
// Parts: assembly (preview only), body, sled, cap, plunger, clip.
// Each printable part comes out already in its print orientation.
//
// Values marked MEASURE are taken from listings or guessed, not from parts in
// hand. Measure yours with calipers before printing the body; everything else
// follows from them. See docs/PEN.md for the build.

part = "assembly";

$fn = 64;

// --- Bought parts (MEASURE) -----------------------------------------------
// Seeed XIAO ESP32-S3 with the Sense expansion board, camera unplugged.
xiao_l = 21.0;        // along the pen, USB-C end first (listing: 21 x 17.8)
xiao_w = 17.8;
xiao_stack = 10.0;    // MEASURE: board + expansion board + USB-C, no camera
usb_offset_t = 0.0;   // MEASURE: USB-C centre from the cavity centre, + = toward the clip side
mic_from_usb = 10.0;  // MEASURE: mic port distance from the USB-C end of the board
// The plug end of the USB-C cable you'll charge with (its plastic overmold).
plug_w = 12.5;        // MEASURE your cable
plug_t = 7.0;

// Good Display GDEM0097T61, 0.97" e-ink. Outline and active area from the
// vendor page; where the active area sits inside the outline is a guess.
epd_l = 30.0;
epd_w = 14.15;
epd_t = 1.0;
epd_active_l = 22.26;
epd_active_w = 10.65;
epd_active_shift = 1.5;  // MEASURE: active-area centre offset away from the FPC end
// DESPI-C02-CV0097 adapter (the panel's boost circuit), mounted behind the panel.
adapter_l = 30.0;     // MEASURE: no dimensions published
adapter_w = 16.0;     // MEASURE
adapter_t = 3.5;      // MEASURE, including parts and solder

// Single-cell LiPo pouch, e.g. size code 801540 (8 x 15 x 40).
lipo_l = 40.0;
lipo_w = 15.0;
lipo_t = 8.0;

// 6 x 6 mm tactile switch, through-hole or SMD.
switch_w = 6.0;
switch_h = 3.6;       // body height without the actuator
switch_total = 5.0;   // with the actuator

// --- Printer fit --------------------------------------------------------------
clear = 0.4;          // gap around loose parts
slide = 0.25;         // per-side gap for parts that slide together
wall = 1.4;
corner_r = 5.0;       // outer corner radius of the cross-section

// --- Derived ----------------------------------------------------------------
in_w = xiao_w + 2 * clear;
in_t = max(xiao_stack, lipo_t, epd_t + adapter_t) + 2 * clear;
out_w = in_w + 2 * wall;
out_t = in_t + 2 * wall;
in_r = corner_r - wall;

tip_l = 12.0;         // tapered end; the USB-C plug goes in here
board_zone = xiao_l + clear;
lipo_zone = lipo_l + clear + 3;  // 3 mm for the battery's wires and protection board
disp_zone = epd_l + 3;
cap_neck_l = 8.0;
cap_head_l = 10.0;

sled_z0 = tip_l + board_zone;
disp_z0 = sled_z0 + lipo_zone;
body_l = disp_z0 + disp_zone + cap_neck_l;

window_l = epd_active_l + 0.6;
window_w = epd_active_w + 0.6;
window_z = disp_z0 + 1.5 + epd_l / 2 + epd_active_shift;  // centre; 1.5 = FPC-end margin in the sled

plunger_d = 6.0;
plunger_play = 0.3;   // free movement above the switch
plunger_out = 2.0;    // how far it stands proud of the cap

clip_w = 6.0;
clip_t = 1.6;
clip_gap = 0.8;       // space for the pocket fabric
clip_l = 46.0;        // below the cap
clip_bump = clip_gap + 0.3;  // 0.3 mm preload against the body

dove_narrow = 4.0;    // dovetail at the cap surface
dove_wide = 5.6;
dove_depth = 1.6;

assert(adapter_w <= in_w - 2 * slide, "e-ink adapter is wider than the inside of the pen");
assert(lipo_w <= in_w - 2 * slide, "battery is wider than the pen's inside");
assert(plug_w + 2 < out_w && plug_t + 2 < out_t, "cable plug doesn't fit the tip");

// --- Shapes -------------------------------------------------------------------

module profile(w, t, r) {
  rr = min(r, t / 2 - 0.01, w / 2 - 0.01);
  offset(r = rr) square([w - 2 * rr, t - 2 * rr], center = true);
}

module slab(w, t, r, h) { linear_extrude(h) profile(w, t, r); }

// Rounded-rectangle hole through a wall, wider on the outside (chamfer c).
module chamfered_window(w, l, depth, c) {
  hull() {
    cube([w, 0.01, l], center = true);
    translate([0, -depth, 0]) cube([w + 2 * c, 0.01, l + 2 * c], center = true);
  }
}

// --- Body: one seamless tube, printed tip down ------------------------------

// Cross-section of the nose at fraction f of its length (0 = tip, 1 = body).
module nose_slice(f, tip_w, tip_t) {
  e = sin(90 * f);  // flat where it meets the body
  translate([0, 0, f * tip_l])
    slab(tip_w + (out_w - tip_w) * e, tip_t + (out_t - tip_t) * e, 2.5 + (corner_r - 2.5) * e, 0.01);
}

module body() {
  tip_out_w = plug_w + 2.4;
  tip_out_t = plug_t + 2.4;
  difference() {
    union() {
      // Nose: eases into the body so the join has no edge.
      steps = 10;
      for (i = [0 : steps - 1]) hull() {
        nose_slice(i / steps, tip_out_w, tip_out_t);
        nose_slice((i + 1) / steps, tip_out_w, tip_out_t);
      }
      translate([0, 0, tip_l]) slab(out_w, out_t, corner_r, body_l - tip_l);
    }
    // Cavity for everything above the tip.
    translate([0, 0, tip_l]) slab(in_w, in_t, in_r, body_l);
    // Tunnel for the cable plug, ending at the board's USB-C port.
    translate([0, usb_offset_t, -1]) slab(plug_w, plug_t, 2, tip_l + 1.01);
    // E-ink window on the front face (-y).
    translate([0, -in_t / 2, window_z]) chamfered_window(window_w, window_l, wall + 0.02, 0.5);
    // Mic: three small holes on the clip side over the mic.
    for (dx = [-1.6, 0, 1.6])
      translate([dx, in_t / 2 - 0.5, tip_l + mic_from_usb]) rotate([-90, 0, 0]) cylinder(d = 1.0, h = wall + 1);
  }
}

// --- Sled: holds battery and display, slides in from the top ----------------

sled_w = in_w - 2 * slide;
sled_t = in_t - 2 * slide;
sled_l = body_l - cap_neck_l - sled_z0;

module sled() {
  pocket_t = epd_t + adapter_t + 0.2;
  pocket_w = max(epd_w, adapter_w) + 0.3;
  difference() {
    slab(sled_w, sled_t, in_r - slide, sled_l);
    // Battery bay, open to the front.
    translate([-(lipo_w + 0.6) / 2, -sled_t / 2 - 1, 1.5])
      cube([lipo_w + 0.6, lipo_t + 0.6 + 1, lipo_l + 0.6]);
    // Display + adapter pocket, open to the front so the panel sits on the window.
    // The adapter rests on the pocket floor; the panel is taped to its front with
    // its active area on the window. The 1.5 mm below is for the flex-cable fold.
    translate([-pocket_w / 2, -sled_t / 2 - 1, lipo_zone + 1.5 - 1.5])
      cube([pocket_w, pocket_t + 1, epd_l + 1.5]);
    // Wire channel down the back, full length.
    translate([-3.5, sled_t / 2 - 2.2, -1]) cube([7, 3, sled_l + 2]);
    // Wire pass between bays and down to the board.
    translate([-3.5, -sled_t / 2 - 1, -1]) cube([7, sled_t + 2, 2.6]);
    translate([-3.5, -1, lipo_zone - 1.5]) cube([7, sled_t, 3]);
  }
}

// --- Cap: click button and clip mount ---------------------------------------

module cap() {
  rib = 0.35;
  neck_w = in_w - 0.2;
  neck_t = in_t - 0.2;
  difference() {
    union() {
      // Neck inside the body, with crush ribs for a press fit.
      slab(neck_w, neck_t, in_r - 0.1, cap_neck_l);
      for (sx = [-1, 1]) translate([sx * neck_w / 2, 0, 0]) cylinder(d = 2 * rib + 1, h = cap_neck_l - 1, $fn = 16);
      // Head outside, with a softened top edge.
      translate([0, 0, cap_neck_l]) slab(out_w, out_t, corner_r, cap_head_l - 1);
      hull() {
        translate([0, 0, cap_neck_l + cap_head_l - 1]) slab(out_w, out_t, corner_r, 0.01);
        translate([0, 0, cap_neck_l + cap_head_l]) slab(out_w - 2, out_t - 2, corner_r - 1, 0.01);
      }
    }
    // Switch pocket at the bottom of the neck.
    translate([-(switch_w + 0.3) / 2, -(switch_w + 0.3) / 2, -0.01]) cube([switch_w + 0.3, switch_w + 0.3, switch_h + 0.2]);
    // Plunger: flange chamber, then the bore out through the top.
    translate([0, 0, switch_h]) cylinder(d = plunger_d + 2.4, h = switch_total - switch_h + 1.6 + plunger_play);
    cylinder(d = plunger_d + 0.4, h = cap_neck_l + cap_head_l + 1);
    // Dovetail for the clip on the back (+y), open toward the neck end.
    translate([0, out_t / 2, cap_neck_l - 0.01])
      linear_extrude(cap_head_l - 1.5)
        polygon([[-dove_narrow / 2, 0.01], [dove_narrow / 2, 0.01], [dove_wide / 2, -dove_depth], [-dove_wide / 2, -dove_depth]]);
  }
}

// Rests on the switch actuator; the flange keeps it captive in the cap.
module plunger() {
  cylinder(d = plunger_d + 2.0, h = 1.6);
  hull() {
    cylinder(d = plunger_d, h = cap_neck_l + cap_head_l + plunger_out - switch_total - 0.6);
    cylinder(d = plunger_d - 1.2, h = cap_neck_l + cap_head_l + plunger_out - switch_total);
  }
}

// --- Clip ----------------------------------------------------------------------
// y = 0 is the back surface of the pen, z = 0 the top face of the body.

module clip() {
  root_l = cap_head_l - 1.5 - 0.3;
  g = 0.15;  // dovetail play
  // Beam, running from the cap head down the back of the body.
  translate([-clip_w / 2, clip_gap, -clip_l]) cube([clip_w, clip_t, clip_l + root_l]);
  // Neck and tongue that slide into the cap's dovetail.
  linear_extrude(root_l)
    polygon([[-dove_narrow / 2 + g, clip_gap + 0.01], [dove_narrow / 2 - g, clip_gap + 0.01],
             [dove_narrow / 2 - g, 0], [dove_wide / 2 - g, -dove_depth + g],
             [-dove_wide / 2 + g, -dove_depth + g], [-dove_narrow / 2 + g, 0]]);
  // Rounded bump at the end that presses on the body.
  translate([0, clip_gap, -clip_l + clip_w / 2]) scale([clip_w / 2, clip_bump, clip_w / 2]) sphere(1);
}

// --- Output --------------------------------------------------------------------

module assembly() {
  color("WhiteSmoke") body();
  color("Silver") translate([0, 0, sled_z0]) sled();
  color("WhiteSmoke") translate([0, 0, body_l - cap_neck_l]) cap();
  color("DimGray") translate([0, 0, body_l - cap_neck_l + switch_total]) plunger();
  color("Gainsboro") translate([0, out_t / 2, body_l]) clip();
  // Stand-ins for the bought parts, to check fit.
  color("SteelBlue", 0.8) translate([-xiao_w / 2, -xiao_stack / 2, tip_l]) cube([xiao_w, xiao_stack, xiao_l]);
  color("Orange", 0.8) translate([-lipo_w / 2, -in_t / 2 + slide + 0.3, sled_z0 + 1.5]) cube([lipo_w, lipo_t, lipo_l]);
  color("Black") translate([-epd_w / 2, -in_t / 2, disp_z0 + 1.5]) cube([epd_w, epd_t, epd_l]);
  color("Green", 0.8) translate([-adapter_w / 2, -in_t / 2 + epd_t, disp_z0 + 1.5]) cube([adapter_w, adapter_t, adapter_l]);
}

if (part == "assembly") assembly();
if (part == "body") body();
if (part == "sled") translate([0, 0, sled_t / 2]) rotate([-90, 0, 0]) sled();  // back face down
if (part == "cap") translate([0, 0, cap_neck_l + cap_head_l]) rotate([180, 0, 0]) cap();  // top down
if (part == "plunger") plunger();
if (part == "clip") translate([0, 0, clip_gap + clip_t]) rotate([-90, 0, 0]) clip();  // outer face down

echo(str("Pen: ", out_w, " x ", out_t, " mm section, body ", body_l, " mm, ",
         body_l + cap_head_l + plunger_out, " mm overall"));
