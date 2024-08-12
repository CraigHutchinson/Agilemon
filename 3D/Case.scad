$fn = 24;

//include<UB.scad>;


// Padding for cushioning/air aroudn screen
TFTpadding = 0.5;

// Border overlap left-Top-Right-Bottom
TFTborder = [5.25, 4.6, 5.25, 9.11];

// Outer TFT panel dimensions + measurement error
TFTsize = [125.4, 99.7, 1.08];
TFTpos = [0,TFTborder.w - TFTborder.y,0];

// window x, y, width, height
TFTwindow = [TFTborder.x, TFTborder.y
    , TFTsize.x-TFTborder.x-TFTborder.z
    , TFTsize.y-TFTborder.y-TFTborder.w];
TFTwindowSize = TFTsize.xy - TFTborder.xy - TFTborder.zw;

// Frame wall width 
WALLwidth = 2.0;
WALLheight = 2.6;

//Frame front
FRONTdepth = 0.8;

// Bevel width in window opening
FRONTwindowBevel = 10;
// Bevel minimum thickness at window opening
FRONTwindowOffset = 0.2;

// TFT flex ribbon exit
TFTribbonWidth = 23.5;
//TFTribbonX = (TFTsize.x - TFTribbonWidth) /2;
TFTribbonPos = TFTpos + [0, TFTsize.y/2, -TFTsize.z];
TFTribbonPadding = 1.0;
TFTribbonBendRadius = 1.3;

// TFT connector
TFTconnectorWidth = 12.6;
TFTconnectorPos = 55.47;

module TFT()
 {  
 color([1, 0, 1] )
   translate(TFTpos)
   linear_extrude(TFTsize.z)
         square( TFTsize.xy, true ); 
 };
 //TFT();
 
 module TFTribbon()
 {
 translate( TFTribbonPos )
     rotate( [0,90,0] )
        cylinder( h = TFTribbonWidth+TFTribbonPadding*2, r = TFTribbonBendRadius, center=true);
 }
 //TFTribbon();
 
module windowOutline()
 {  
 translate( TFTpos )
    translate(-TFTsize.xy/2 + TFTborder.xy)
         square( TFTwindowSize, false ); 
 };

module frontFrameOutline() 
{
translate( -TFTsize.xy/2)
    offset(WALLwidth + TFTpadding) 
         square( TFTsize.xy + TFTpos, false ); 
}
         
         
 module wallOutline()
 {
    difference()
    {
         frontFrameOutline();
         translate(TFTpos) 
            offset(delta=TFTpadding) square( TFTsize.xy, true ); 
    }
 }
 
 module windowCutout()
 {
    scalet = [1,1] + [FRONTwindowBevel/TFTwindowSize.x
                     ,FRONTwindowBevel/TFTwindowSize.y];
  
  translate([0,0,FRONTwindowOffset])
   linear_extrude( height = FRONTdepth+0.01, scale = scalet )  windowOutline();

 }
 // windowCutout();

  
 module walls()
 {
     difference()
     {
        translate( [0,0,-WALLheight] )
            linear_extrude(WALLheight+TFTsize.z) wallOutline();
            
        TFTribbon();
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
                frontFrameOutline();
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