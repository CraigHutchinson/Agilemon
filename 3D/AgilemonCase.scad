$fn = 24;
include <BOSL2/std.scad>


// Datasheet: https://www.data-modul.com/sites/default/files/products/AB1024-EGA-specification-12051791.pdf

// Padding for cushioning/air aroudn screen
TFTpadding = 0.33;

// Border overlap left-Top-Right-Bottom
TFTborder = [5.25, 4.6, 5.25, 9.11];

// Outer TFT panel dimensions + measurement error
TFTsize = [125.4, 99.7, 1.08];

// TFT Position offset within frame to center the 'image' in the housing
TFTpos = [
 TFTborder.x - TFTborder.z
,TFTborder.w - TFTborder.y
,0] / 2;

// window x, y, width, height
TFTwindowPadding = 0.5;
TFTwindowSize = TFTsize.xy - TFTborder.xy - TFTborder.zw
 + [TFTwindowPadding,TFTwindowPadding];

// Frame wall width 
WALLwidth = 1.6;
WALLheight = 1.6;

//Frame front
FRONTdepth = 1.2;
FRONTframeWidth = [
 WALLwidth+ (TFTpos.x<0 ? -TFTpos.x*2:0)
,WALLwidth+ (TFTpos.y<0 ? -TFTpos.y*2:0)
,WALLwidth+ (TFTpos.x>0 ? TFTpos.x*2:0)
,WALLwidth+ (TFTpos.y>0 ? TFTpos.y*2:0)];

// Bevel width in window opening
FRONTwindowBevel = 10;
// Bevel minimum thickness at window opening
FRONTwindowOffset = 0.4;

// TFT flex ribbon exit
TFTribbonWidth = 23.5;
//TFTribbonX = (TFTsize.x - TFTribbonWidth) /2;
// Allow lateral position padding
TFTribbonWidthPad = 1.0;
// Allow some space tolerance on all axes
TFTribbonTolerance = 0.1;
TFTribbonTickness = 0.3;
TFTribbonBendRadius = 1.0;
TFTribbonPos = TFTpos + [0, TFTsize.y/2,0.1];
TFTribbonLength = 24.01;
TFTribbonConnectorLength = 3.67;

//306070 = GOT 2100 mAh
//306090 = TBD 3000 mAh
//3098140 ??
//4285104 = PERFECT!
//606090 = Common 5000 mAh is excessive!
batterySize = [60,70,3];

// Accomodate expansion of li-on battery, 5% to 10%
// https://electronics.stackexchange.com/questions/419408/how-much-do-lithium-polymer-batteries-expand-in-volume
batteryExpansionFactor=0.1;

// TFT ribbon connector width
TFTconnectorWidth = 12.6;
// Offset of TFT connector from screen due to ribbon routing (ToDO: Calculate full based on battery width etc!)
TFTconnectorRibbonOffset = -[0,TFTribbonLength - (PI * TFTribbonBendRadius ) - TFTribbonConnectorLength - 0.25, batterySize.z * (1+batteryExpansionFactor)];
// TFT ribbon connector position (note it is off-center to screen)
TFTconnectorPos = TFTpos + [TFTsize.x/2, TFTsize.y/2, 0]- [55.47+TFTconnectorWidth/2,0,0] + TFTconnectorRibbonOffset;

//The actual contact dpeth... should be less
TFTconnectorDepth = 3.67;

TFTribbonBendStartAngle = 163;

pcbStl = "LILYGO T5 2.13inch v2.3.2 DEPG0213BN.stl";
// Measured position to center of screen ribbon connector 
pcbStlRibbonPos = [-62,-(40.23 + 16.150/2),0];
// Rotattion to put pcb STL into corrected rorientation
pcbStlRotate = [180,0,90];

module TFTribbon( widthPad = 0, offset = 0 )
{
//Turtle3d https://github.com/BelfrySCAD/BOSL2/wiki/turtle3d.scad#function-turtle3d

 translate( TFTribbonPos )
 {
 //TFTribbonLength= 24.01
 coplanarLength = 5;
    bendState = turtle([
    "setdir",[0,1]
    , "arcleft", TFTribbonBendRadius, TFTribbonBendStartAngle]
    , full_state=true);
    bendLength = path_length(bendState[0]);
    linearLength = TFTribbonLength-TFTconnectorDepth-bendLength;
    
    path= turtle([ "move", linearLength-coplanarLength, "arcleft", TFTribbonBendRadius,180-TFTribbonBendStartAngle,  "move", coplanarLength], state=bendState);
    
    yrot(-90)
    linear_extrude(TFTribbonWidth+widthPad*2,center=true)
    offset(offset)
    stroke(path,width=TFTribbonTickness, endcaps="square");    
    }
}

 translate( TFTconnectorPos )
    rotate(pcbStlRotate)
    translate(pcbStlRibbonPos)
    % import(pcbStl);
module tftOutline()
{
 
   translate(TFTpos)
         square( TFTsize.xy, center=true ); 
}

module windowOutline()
 {  
 //translate( TFTpos ) 
    square( TFTwindowSize, center=true ); 
 };


module frameOutline() 
{
    //offset(WALLwidth + TFTpadding) 
    square( TFTsize.xy + FRONTframeWidth.xy + FRONTframeWidth.zw, center=true ); 
}
       
       
module battery()
{

down(batterySize.z * batteryExpansionFactor)
right(batterySize.y/4)
 cube(batterySize,anchor=TOP);
}
% battery();

module TFT()
 {  
   color([1, 0, 1, 0.2] ) linear_extrude(TFTsize.z) tftOutline();
 };
% TFT();
% TFTribbon();
   
         
 module tftSpaceOutline()
 {    
    offset(delta=TFTpadding) tftOutline();
 }
 
 module wallOutline()
 {
    difference()
    {
         frameOutline();
         tftSpaceOutline();
    }
 }
 
 module windowCutout()
 {
    scaleT = [1,1] + [FRONTwindowBevel/TFTwindowSize.x
                     ,FRONTwindowBevel/TFTwindowSize.y];
  
  up(FRONTwindowOffset)
   linear_extrude( height = FRONTdepth+0.01, scale = scaleT )  windowOutline();

 }
 // windowCutout();

module tftClips()
{
    TFTclipStep = TFTpadding*2 + 0.1;
    TFTclipDepth = 0.75;
    scalet = [1,1] - [TFTclipStep/TFTsize.x
                     ,TFTclipStep/TFTsize.y];
    
    intersection()
    {
        difference()
        {
            color([1, 0, 0] ) translate( [0, 0, -TFTsize.z-TFTclipDepth]) 
            difference()
            {
              linear_extrude(height=TFTclipDepth + TFTsize.z) tftSpaceOutline();
              translate([0,0,-0.2]) linear_extrude(height=TFTclipDepth + 0.3 + TFTsize.z, scale=scalet) tftSpaceOutline();
              };
               scale( [2,2,1.1] ) TFT();
        }
        
        union(){
        translate( [TFTsize.x/3, 0, 0]) cube( [5, TFTsize.y+10, 10], center=true);
        translate( [-TFTsize.x/3, 0, 0]) cube( [5, TFTsize.y+10, 10], center=true);
        }
    }
}
  
 module walls()
 {
 union()
 {
     difference()
     {
        translate( [0,0,-WALLheight] )
            linear_extrude(WALLheight+TFTsize.z) wallOutline();
        
        // SUbtract hull of TFTribbon for routing in housing
        hull() TFTribbon( TFTribbonWidthPad, TFTribbonTolerance );
     }
     
     tftClips();
}
 }
 
 module frontFrame()
 {
    translate( [0,0,TFTsize.z] )
      difference()
      {
         linear_extrude( FRONTdepth ) 
           difference()
             {
                frameOutline();
                windowOutline();
            }
        windowCutout();
    }
 }
 
// TFT();
//TFTribbon();

module it()
{
  union(){
    walls();
    frontFrame();
  }
}
it();