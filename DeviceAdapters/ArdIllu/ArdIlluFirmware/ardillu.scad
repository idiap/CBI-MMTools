/**
* Copyright (c) 2021 Idiap Research Institute, http://www.idiap.ch/
* Written by François Marelli <francois.marelli@idiap.ch>
* 
* This file is part of CBI-MMTools.
* 
* CBI-MMTools is free software: you can redistribute it and/or modify
* it under the terms of the 3-Clause BSD License.
* 
* CBI-MMTools is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* 3-Clause BSD License for more details.
* 
* You should have received a copy of the 3-Clause BSD License along
* with CBI-MMTools. If not, see https://opensource.org/licenses/BSD-3-Clause.
* 
* SPDX-License-Identifier: BSD-3-Clause 
*/

$fn = 50;

// stripboard
w = 80;
h = 50;

// margins
mh = 2;
mw = 10;
bm = 15;

// corners
c = 8;
cd = 2.8;
cdl = 3.1;

// box
W = w + 2 * (mw+ c);
H = h + 2 * mh + bm;

T = 50;
wall_thick = 2;
e = 2.5;

// feet
tl = 2;
t = T - tl;
td = 20;
fh = 5;

thorlabs_feet = true;
hf = 8;

// printing
tol=0.5;
space = 30;

 module foot(c, cd, t, td){
     translate([0, 0, t/2])
    difference(){
        cube([c, c, t], center=true);
        translate([0, 0, t-td]) cylinder(d=cd, h=t, center=true);
    }
}

module feet(W, H, c, cd, t, td){
    
    translate([(W-c)/2, (H-c)/2, 0 ]) foot(c, cd, t, td);
    translate([(W-c)/2, -(H-c)/2,  0]) foot(c, cd, t, td);
    translate([-(W-c)/2, (H-c)/2,  0]) foot(c, cd, t, td);
    translate([-(W-c)/2, -(H-c)/2,  0]) foot(c, cd, t, td);
    
}

 module footc(c, cd, t, td){
     translate([0, 0, t/2])
    difference(){
        cylinder(d=c, h=t, center=true);
        translate([0, 0, t-td]) cylinder(d=cd, h=t, center=true);
    }
}

module feetc(x, y, c, cd, t, td){
    translate([x, y, 0])footc(c, cd, t, td);
    translate([-x, y, 0])footc(c, cd, t, td);
    translate([-x, -y, 0])footc(c, cd, t, td);
    translate([x, -y, 0])footc(c, cd, t, td);
}

module box(W, H){

    difference(){
    linear_extrude(height=T)
    offset(delta=wall_thick, chamfer=true)
    square([W, H], center=true);
       translate([0, 0, e+T/2]) cube([W, H, T], center=true);
    }
    
    
    feet(W, H, c, cd, t, td);
    translate([0, bm/2, 0])   feetc(w/2-4.5, h/2-4.5, 7, cd, fh +e, fh);
    
    translate([-(w/2-4.5), (h/2-4.5)+bm/2, 0]) cylinder(d=cd, h=fh+3*e);
    translate([-(w/2-4.5), -(h/2-4.5)+bm/2, 0]) cylinder(d=cd, h=fh+3*e);
    translate([(w/2-4.5), -(h/2-4.5)+bm/2, 0]) cylinder(d=cd, h=fh+3*e);
    
    
     if (thorlabs_feet){
        translate([0.3*W, 0.5*H+6+wall_thick, 0]) 
        difference(){
            union(){
                cylinder(h=hf, d=12);
                translate([-6, -6, 0])cube([12, 6, hf]);
            }
            translate([0, 0, -1]) cylinder(h=hf+2, d=6+tol);
        }
        
        translate([0.3*W - 50, 0.5*H+6+wall_thick, 0]) 
        difference(){
            union(){
                cylinder(h=hf, d=12);
                translate([-6, -6, 0])cube([12, 6, hf]);
            }
            translate([0, 0, -1]) cylinder(h=hf+2, d=6+tol);
        }
    }
}

 module lid(W, H){
     
     module hole(){
     translate([(W-c)/2, (H-c)/2, 5]) cylinder(d=cdl, h=tl+10, center=true);
     }
     
     difference(){
         union(){
            linear_extrude(height=e)
            offset(delta=wall_thick, chamfer=true)
            square([W, H], center=true);         
             
            translate([0, 0, e - tol]) feet(W-tol, H-tol, c, cd, tl, -1);
         }
         
         
         union(){
             hole();
             mirror([1, 0, 0]) hole();
             mirror([0, 1, 0]) hole();
             mirror([1, 0, 0]) mirror([0, 1, 0]) hole();
         }
     }
 }
 
 module bnc(){
 rotate(90, [1, 0, 0]) {
     difference(){
     cylinder(d=9.8, h=4*wall_thick, center=true);
     translate([-9.8/2, -9.8/2, -2.5*wall_thick]) cube([9.8, 9.8-8.85, 5*wall_thick]);}
     }
 }
 
 
 usbz = e+fh + 15;
 portz = usbz+10;
 
 module ports(){ 
 translate([-w/2+4.5-2.54+7.6, H/2, usbz])
    cube([12, 4*wall_thick, 10], center=true);
 
 // POWER
 translate([W/2-20, H/2, portz]){
 rotate(90, [1, 0, 0]) cylinder(d=9.5, h=4*wall_thick, center=true);     
translate([0, wall_thick/2, 10]) rotate(180, [0, 0, 1]) rotate(90, [1, 0, 0]) linear_extrude(height=wall_thick) text(valign="center", halign="center", size=6, "\u26a1", font="DejaVu Sans");
 }
 
 // Power Switch 
translate([10, H/2, portz])
rotate(180, [0, 0, 1]){
    rotate(180, [0, 1, 0]) rotate(-90, [1, 0, 0]) {
     difference(){     
    cylinder(d=6.35, h=4*wall_thick, center=true);
     translate([0, -(6.35-0.55)/2, 0]) cube([0.85, 0.55, 5*wall_thick], center=true);}     
     translate([0,-6.1, 0]) cylinder(d=2.39, h=4*wall_thick, center=true);
     translate([0,10, 0]) cylinder(d=3.05, h=4*wall_thick, center=true);
     }
 }
 
 // LED
 translate([w/2-10,- H/2, portz]){
 rotate(90, [1, 0, 0]) cylinder(d=9.5, h=4*wall_thick, center=true); 
  translate([0, 0, 10]) rotate(-90, [1, 0, 0]) cylinder(d=3.05, h=4*wall_thick, center=true);
translate([-0.1, -wall_thick/2, 9.9]) rotate(90, [1, 0, 0]) linear_extrude(height=wall_thick) text(valign="center", halign="center", size=6.5, "\u2600", font="DejaVu Sans");
 }
 
 // BNC
translate([W/2, -10, portz]) rotate(90, [0, 0, 1]) {
    bnc(); 
    translate([0, -wall_thick/2, 10]) rotate(90, [1, 0, 0]) linear_extrude(height=wall_thick) text(valign="center", halign="center", size=4, "1/0", font="DejaVu Sans");
}

translate([W/2, 10, portz]) rotate(90, [0, 0, 1]) {
    bnc(); 
    translate([0, -wall_thick/2, 10]) rotate(90, [1, 0, 0]) linear_extrude(height=wall_thick) text(valign="center", halign="center", size=6, "\u223f", font="DejaVu Sans");
}


// Potentiometer
translate([0, -H/2, portz])
rotate(180, [0, 0, 1])
rotate(90, [1, 0, 0]){
translate([7.8, 0, 0]) cylinder(d=3, h=4*wall_thick, center=true);
cylinder(d=8, h=4*wall_thick, center=true);    
}

// Button
translate([-30, -H/2, portz])
rotate(90, [1, 0, 0])
cylinder(d=12.2, h=4*wall_thick, center=true);

// Switch
translate([-W/2, 0, portz])
rotate(-90, [0, 0, 1]){
    rotate(-90, [0, 1, 0]) rotate(-90, [1, 0, 0]) {
     difference(){     
    cylinder(d=6.35, h=4*wall_thick, center=true);
     translate([0, -(6.35-0.55)/2, 0]) cube([0.85, 0.55, 5*wall_thick], center=true);}     
     translate([0,-6.1, 0]) cylinder(d=2.39, h=4*wall_thick, center=true);
     }
     translate([-7, -wall_thick/2, 0]) rotate(90, [1, 0, 0]) linear_extrude(height=wall_thick) text(valign="center", halign="right", size=4, "Soft.", font="DejaVu Sans");
     translate([7, -wall_thick/2, 0]) rotate(90, [1, 0, 0]) linear_extrude(height=wall_thick) text(valign="center", halign="left", size=4, "Ext.", font="DejaVu Sans");
 }
}


difference(){
    box(W, H);
    ports();
}
 
translate([0, H+space, 0]) lid(W, H);