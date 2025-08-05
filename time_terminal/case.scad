/* [Rendering options] */
// Show placeholder PCB in OpenSCAD preview
show_pcb = false;
// Lid mounting method
lid_model = "cap"; // [cap, inner-fit]
// Conditional rendering
render = "case"; // [all, case, lid]


/* [Dimensions] */
// Height of the PCB mounting stand-offs between the bottom of the case and the PCB
standoff_height = 5;
// PCB thickness
pcb_thickness = 1.6;
// Bottom layer thickness
floor_height = 1.2;
// Case wall thickness
wall_thickness = 1.2;
// Space between the top of the PCB and the top of the case
headroom = 25.0;

/* [M3 screws] */
// Outer diameter for the insert
insert_M3_diameter = 3.77;
// Depth of the insert
insert_M3_depth = 4.5;

/* [M2.5 screws] */
// Outer diameter for the insert
insert_M2_5_diameter = 3.27;
// Depth of the insert
insert_M2_5_depth = 3.75;

/* [Hidden] */
$fa=$preview ? 10 : 4;
$fs=0.2;
inner_height = floor_height + standoff_height + pcb_thickness + headroom;

module wall (thickness, height) {
    linear_extrude(height, convexity=10) {
        difference() {
            offset(r=thickness)
                children();
            children();
        }
    }
}

module bottom(thickness, height) {
    linear_extrude(height, convexity=3) {
        offset(r=thickness)
            children();
    }
}

module lid(thickness, height, edge) {
    linear_extrude(height, convexity=10) {
        offset(r=thickness)
            children();
    }
    translate([0,0,-edge])
    difference() {
        linear_extrude(edge, convexity=10) {
                offset(r=-0.2)
                children();
        }
        translate([0,0, -0.5])
         linear_extrude(edge+1, convexity=10) {
                offset(r=-1.2)
                children();
        }
    }
}


module box(wall_thick, bottom_layers, height) {
    if (render == "all" || render == "case") {
        translate([0,0, bottom_layers])
            wall(wall_thick, height) children();
        bottom(wall_thick, bottom_layers) children();
    }
    
    if (render == "all" || render == "lid") {
        translate([0, 0, height+bottom_layers+0.1])
        lid(wall_thick, bottom_layers, lid_model == "inner-fit" ? headroom-2.5: bottom_layers) 
            children();
    }
}

module mount(drill, space, height) {
    translate([0,0,height/2])
        difference() {
            cylinder(h=height, r=(space/2), center=true);
            cylinder(h=(height*2), r=(drill/2), center=true);
            
            translate([0, 0, height/2+0.01])
                children();
        }
        
}

module connector(min_x, min_y, max_x, max_y, height) {
    size_x = max_x - min_x;
    size_y = max_y - min_y;
    translate([(min_x + max_x)/2, (min_y + max_y)/2, height/2])
        cube([size_x, size_y, height], center=true);
}

module Cutout_CUI_SJ1_353x_substract() {
    r = 1;
    translate([-5, 0, 1.7])
    rotate([0, 90, 0])
        cylinder(10, r, r);
}

module Cutout_CUI_SJ1_353x_substract1() {
    r = 3;
    translate([-5, 0, 1.7])
    rotate([0, 90, 0])
        cylinder(10, r, r);
}

module CaseCorner(size, hole_diameter, head_diameter, head_height) {
    translate([0, 0, -floor_height])
    difference() {
        union() {
            cylinder(inner_height, size/2, size/2);
            translate([-size, 0, inner_height/2])
                cube([size*2, size, inner_height], center=true);
        }

        cylinder(inner_height+1, hole_diameter/2, hole_diameter/2);

        translate([0, 0, floor_height])
            CaseCorner_substract(size, hole_diameter, head_diameter, head_height);

        translate([0, 0, inner_height+0.01])
            children();

    }
}

module CaseCorner_substract(size, hole_diameter, head_diameter, head_height) {
    translate([0, 0, 0.11]) {
        cylinder(inner_height+floor_height, hole_diameter/2, hole_diameter/2);
        translate([0, 0, inner_height+floor_height-head_height])
            cylinder(head_height, hole_diameter/2, head_diameter/2);
    }
}
module Cutout_TypeC_substract() {
    width = 10;
    length = 10;
    height = 3.5;
    translate([-length/2, 0, height/2])
    rotate([0,90,0])
        union() {
            translate([0, -(width/2 - height/2), 0])
                cylinder(length, height/2, height/2);
            translate([0, (width/2 - height/2), 0])
                cylinder(length, height/2, height/2);
            translate([0, 0, length/2])
                cube([height, width-height, length], center=true);
        }
}
module pcb() {
    thickness = 1.6;

    color("#009900")
    difference() {
        linear_extrude(thickness) {
            polygon(points = [[159.098351,122.250001], [159.0413875,122.9011875], [158.87218750000002,123.532575], [158.5959625,124.125], [158.221,124.6604625], [157.7588125,125.12265], [157.22335,125.4976125], [156.63092500000002,125.7738375], [155.9995375,125.9430375], [155.348351,126.000001], [63.25,126], [62.6633875,125.9538375], [62.091175,125.816475], [61.5475375,125.59128749999999], [61.0457875,125.283825], [60.5983375,124.9016625], [60.216175,124.4542125], [59.9087125,123.9524625], [59.683525,123.408825], [59.5461625,122.8366125], [59.5,122.25], [59.499999,57.749999], [59.5569625,57.0988125], [59.7261625,56.467425], [60.0023875,55.875], [60.37735,55.3395375], [60.8395375,54.87735], [61.375,54.5023875], [61.967425,54.2261625], [62.5988125,54.0569625], [63.249999,53.999999], [155.34835,54], [155.9349625,54.0461625], [156.50717500000002,54.183525], [157.0508125,54.4087125], [157.55256250000002,54.716175], [158.0000125,55.0983375], [158.38217500000002,55.5457875], [158.6896375,56.0475375], [158.914825,56.591175], [159.0521875,57.1633875], [159.09835,57.75], [159.098351,122.250001]]);
        }
    translate([63.250001, 122.249999, -1])
        cylinder(thickness+2, 1.0999999999999943, 1.0999999999999943);
    translate([63.25, 57.75, -1])
        cylinder(thickness+2, 1.1000000000000014, 1.1000000000000014);
    translate([155.34835, 57.75, -1])
        cylinder(thickness+2, 1.1000000000000014, 1.1000000000000014);
    translate([155.34835, 122.25, -1])
        cylinder(thickness+2, 1.0999999999999943, 1.0999999999999943);
    }
}

module case_outline() {
    polygon(points = [[57.75,45.5], [153.5,45.5], [154.4160625,45.54906], [155.3127175,45.743179999999995], [156.1670525,46.0774275], [156.9573325,46.5432925], [157.66349,47.1288725], [158.2675275,47.819332499999994], [158.7540925,48.597134999999994], [159.1107075,49.4423275], [159.3284025,50.33352], [159.401596,51.248001], [159.401596,128.75], [159.32677941,129.6750127096], [159.1042744862,130.5760252761], [158.73978310430002,131.4295555451], [158.2429168324,132.2133875568], [157.6265102794,132.907147089], [156.90662109500002,133.4928196458], [156.1018968724,133.955210892], [155.2334024854,134.2822344246], [154.323584111,134.4654297615], [153.396165,134.499999], [57.749999,134.505431], [56.8249872904,134.43060941], [55.9239747239,134.2081044862], [55.0704444549,133.8436131043], [54.2866124432,133.3467468324], [53.592852911,132.7303402794], [53.0071803542,132.010451095], [52.544789108,131.2057268724], [52.2177655754,130.3372324854], [52.0345702385,129.427414111], [52,128.5], [52,51.25], [52.0707825,50.3505275], [52.281405,49.473135], [52.6266925,48.6395575], [53.098135,47.8702075], [53.6841175,47.1841175], [54.3702075,46.598135], [55.1395575,46.1266925], [55.973135,45.781405], [56.8505275,45.5707825], [57.75,45.5]]);
}

module Insert_M3() {
    translate([0, 0, -insert_M3_depth])
        cylinder(insert_M3_depth, insert_M3_diameter/2, insert_M3_diameter/2);
    translate([0, 0, -0.3])
        cylinder(0.3, insert_M3_diameter/2, insert_M3_diameter/2+0.3);
}

module Insert_M2_5() {
    translate([0, 0, -insert_M2_5_depth])
        cylinder(insert_M2_5_depth, insert_M2_5_diameter/2, insert_M2_5_diameter/2);
    translate([0, 0, -0.3])
        cylinder(0.3, insert_M2_5_diameter/2, insert_M2_5_diameter/2+0.3);
}

rotate([render == "lid" ? 180 : 0, 0, 0])
scale([1, -1, 1])
translate([-105.700798, -90.0027155, 0]) {
    pcb_top = floor_height + standoff_height + pcb_thickness;

    difference() {
        box(wall_thickness, floor_height, inner_height) {
            case_outline();
        }

    translate([140.35, 91.75, inner_height])
        cylinder(floor_height+2, 1.9849430000000154, 1.9849430000000154);
    translate([140.75, 109.75, inner_height])
        cylinder(floor_height+2, 5.284174000000007, 5.284174000000007);
    translate([74.95, 104.225, inner_height+floor_height])
        cube([26.900000000000006, 18.950000000000003, floor_height + 2], center=true);
    translate([108.15, 121.225, inner_height+floor_height])
        cube([3.0, 1.8500000000000085, floor_height + 2], center=true);
    translate([115.95, 121.225, inner_height+floor_height])
        cube([3.0, 1.8500000000000085, floor_height + 2], center=true);
    // J1 Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical 
    translate([103.25, 113, pcb_top])
    rotate([0, 0, -90])
        #connector(-2.5,-11.05,57.3,28.35,7.2);

    // J2 footprints:128x64OLED 
    translate([74.58, 103.85, pcb_top])
    rotate([0, 0, 0])
        #connector(-13.4,-12.3,14,15,7.2);

    // Substract: footprints:corner
    translate([56.569849, 49.819849, floor_height])
    rotate([0, 0, 45])
        CaseCorner_substract(size=5.6, hole_diameter=3, head_diameter=5.6, head_height=1.65);

    // Substract: footprints:corner
    translate([154.930152, 49.819849, floor_height])
    rotate([0, 0, 135])
        CaseCorner_substract(size=5.6, hole_diameter=3, head_diameter=5.6, head_height=1.65);

    // Substract: footprints:corner
    translate([56.5, 129.75, floor_height])
    rotate([0, 0, -45])
        CaseCorner_substract(size=5.6, hole_diameter=3, head_diameter=5.6, head_height=1.65);

    // Substract: footprints:corner
    translate([154.930152, 129.680152, floor_height])
    rotate([0, 0, -135])
        CaseCorner_substract(size=5.6, hole_diameter=3, head_diameter=5.6, head_height=1.65);

    // Substract: footprints:cutout
    translate([158.5, 64.25, pcb_top])
        Cutout_CUI_SJ1_353x_substract();
        
        translate([50.5, 90, pcb_top-15])
        Cutout_CUI_SJ1_353x_substract1();

    // Substract: footprints:usbc
    translate([159.65, 78.6, pcb_top])
        Cutout_TypeC_substract();

    }

    if (show_pcb && $preview) {
        translate([0, 0, floor_height + standoff_height])
            pcb();
    }

    if (render == "all" || render == "case") {
        // H3 [('M2.5', 2.5)]
        translate([63.250001, 122.249999, floor_height])
        mount(2.2, 4.9, standoff_height)
            Insert_M2_5();
        // H1 [('M2.5', 2.5)]
        translate([63.25, 57.75, floor_height])
        mount(2.2, 4.9, standoff_height)
            Insert_M2_5();
        // H2 [('M2.5', 2.5)]
        translate([155.34835, 57.75, floor_height])
        mount(2.2, 4.9, standoff_height)
            Insert_M2_5();
        // H4 [('M2.5', 2.5)]
        translate([155.34835, 122.25, floor_height])
        mount(2.2, 4.9, standoff_height)
            Insert_M2_5();
        intersection() {
            translate([0, 0, floor_height])
            linear_extrude(inner_height)
                case_outline();

            union() {

            // footprints:corner
            translate([56.569849, 49.819849, floor_height])
            rotate([0, 0, 45])
                CaseCorner(size=5.6, hole_diameter=3, head_diameter=5.6, head_height=1.65)
                    Insert_M3();

            // footprints:corner
            translate([154.930152, 49.819849, floor_height])
            rotate([0, 0, 135])
                CaseCorner(size=5.6, hole_diameter=3, head_diameter=5.6, head_height=1.65)
                    Insert_M3();

            // footprints:corner
            translate([56.5, 129.75, floor_height])
            rotate([0, 0, -45])
                CaseCorner(size=5.6, hole_diameter=3, head_diameter=5.6, head_height=1.65)
                    Insert_M3();

            // footprints:corner
            translate([154.930152, 129.680152, floor_height])
            rotate([0, 0, -135])
                CaseCorner(size=5.6, hole_diameter=3, head_diameter=5.6, head_height=1.65)
                    Insert_M3();

            }
        }
    }
}
