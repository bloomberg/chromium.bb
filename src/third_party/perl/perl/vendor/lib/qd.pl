#!/usr/local/bin/perl

# $Id: qd.pl,v 1.1.1.1 2001-12-06 23:25:48 lstein Exp $

# This is a package of routines that let you create Macintosh 
# PICT files from within perl.  It implements a subset of Quickdraw
# drawing commands, primarily those related to line drawing, rectangles,
# ovals, polygons, and text.  Flagrantly absent are: regions and the 
# snazzy color transfer modes.  Regions are absent because they were
# more trouble than I had time for, and the transfer modes because I
# never use them.  (The latter shouldn't be too hard to add.)  Also 
# missing are the pixmap commands.  If you want to do pixmaps, you
# should be using the ppm utilities.

# A QUICK TUTORIAL ON QUICKDRAW
# 
# Quickdraw is not Postscript.  You cannot write routines in it or get
# (any useful) information out of it.  Quickdraw pictures are a series of
# drawing commands, concatenated together in a binary format.
#
# A Macintosh picture consists of a header describing the size of the
# picture and its bounding rectangle, followed by a series of drawing
# commands, followed by a termination code.  This perl library is
# modeled closely on the way that you would draw a picture on the Mac.
# First you open the picture with the &qd'OpenPicture() command.  This
# initializes some data structures.  Then you call a series of drawing
# subroutines, such as &qd'TextFont(), &qd'MoveTo(), &qd'DrawString().
# These routines append their data to the growing (but still private)
# picture.  You then close the picture with &qd'ClosePicture.  This
# returns a scalar variable containing the binary picture data.

# RECTANGLES
#
# To open a picture you need to define a rectangle that will serve as
# its frame and will define its drawing area.  The rectangle is (of 
# course) a binary structure.  The following utilities allow you to
# create and manipulate rectangles:
#
#   &qd'SetRect(*myRect,left,top,right,bottom); # Set the sides of $myRect
#   &qd'OffsetRect(*myRect,deltaH,deltaV);      # Shift the rectangle as indicated
#   &qd'InsetRect(*myRect,deltaH,deltaV);       # Shrink rectangle by size indicated


# OPENING A PICTURE
#
# Pass a previously-defined rectangle to the routine OpenPicture.  Only one picture
# may be open at a time.  The rectangle defines the drawing area in pixels.
# A printer page is 8.5 x 11 inches, at 72 pixels per inch = 612 x 792 pixels.
#
#   &qd'OpenPicture($myRect);
#
# You will next very likely want to set the clipping rectangle to the same rectangle
# you used to open the picture with.  Clipping rectangles limit quickdraw's drawing
# to the area within the rectangle.  Even if you don't use clipping, however, it's a
# good idea to define the rectangle because some drawing programs behave eratically
# when displaying unclipped pictures.
# 
# You then issue drawing commands.  When you're done you can get the picture data with
# something like $pictData = &qd'ClosePicture;

# 
# SETTING THE FOREGROUND AND BACKGROUND COLORS
#
# The foreground color is the color of the ink when a "frame" or "paint" command
# is given.  The background color is the color of the erased area when an "erase"
# command is given.  The defaults are black and white.  The colors can be changed
# in either of two ways:
#
#  1. The "old" 8-color system: black, white, red, green, blue, cyan, magenta, yellow
#     Call the routines &qd'FgColor() and &qd'BgColor() with one of the constants
#     $qd'REDCOLOR,$qd'GREENCOLOR, etc.  This gives you a limited number of highly
#     satured colors.
#
#  2. The new 24-bit color system.  Call the routines &qd'RGBForeColor() and 
#     &qd'RGBBackColor(), passing the routines the red, green and blue components
#     of the color.  These components are two-byte unsigned integers, so you can choose
#     any value between 0x000 and 0xFFFF.  Higher is darker, so:
#     (0x0000,0x0000,0x0000) = BLACK
#     (0xFFFFF,0xFFFF,0xFFFF) = WHITE
#     (0xFFFFF,0x0000,0x0000) = PURE RED
#     etc.


# SETTING THE PATTERN
#
# Like colors, the drawing commands use the current pattern, a 32 row x 32 column 
# bit array that defines the pattern of the "ink".
# The default pattern is $qd'BLACK, which is solid black.  The only
# other pattern I've defined is $qd'GRAY, which is a 50% checkerboard.  You
# might want to define others.
#
# The current pattern is set using &qd'PenPat($myPattern).


# LINE DRAWING
#
# Quickdraw has the concept of the "current point" of the pen.  Generally
# you move the pen to a point and then start drawing.  The next time you draw,
# the pen will be wherever the last drawing command left it.  In addition, the
# pen has a width, a pattern and a color.  In the below descriptions, 
# h=horizontal, v=vertical
#
# &qd'MoveTo(h,v)           # Move to indicated coordinates (0,0 is upper left of picture)
# &qd'LineTo(h,v)           # Draw from current position to indicated position
# &qd'Line(dh,dv)           # Draw a line dh pixels horizontally, dv pixels vertically,
#                             starting at current position
# &qd'PenSize(h,v)          # Set the size of the pen to h pixels wide, v pixels high


# PEN SCALING
#
# The original quickdraw was incapable of drawing at higher than the screen resolution,
# so even if the PenSize is set to (1,1) the lines will appear chunky when printed out
# on the laserwriter (which has four times the resolution of the screen).  Call 
# &qd'Scale(1,4) to fix this problem by shrinking the pen down to a quarter of its
# (1,1) size.
#
# &qd'Scale(numerator,denominator) # Scale the pen by the fraction numerator/denominator


# TEXT
#
# &qd'TextFont(fontCode)    # Set the current font to indicated code.  Currently
#                             defined fonts are $qd'TIMES, $qd'NEWCENTURYSCHOOLBK,
#                             $qd'SYMBOL, $qd'HELVETICA, and $qd'COURIER.
#
# &qd'TextSize(size)        # Set the current font size (in points).  12 point is typical
#
# &qd'TextFace(attributes)  # Set one or more font style attributes.  Currently defined
#                             are $qd'PLAIN, $qd'BOLD, $qd'ITALIC, $qd'UNDERLINE, and
#                             can be used in combination: 
#                             &qd'TextFace($qd'BOLD + $qd'ITALIC);
#
# &qd'DrawString(string)    # Draw the indicated text.  It will be drawn from the
#                             current pen location.  Word wrap is NOT supported.
#                             Rotated text is NOT supported.
#
# &qd'TextWidth(string)     # This will return an approximate width for the string
#                             when it is printed in the current size, font and face.
#                             Unfortunately, since perl has no access to the Macintosh
#                             font description tables, the number returned by this
#                             routine will be wildly inaccurate at best.
#                             However, if you have X11R5 bdf fonts installed, we look 
#                             in the directory $qd'X11FONTS in order to find a bdf metrics
#                             font to use.  This will give you extremely accurate measurements.
#                             Please set this variable to whatever is correct for your local
#                             system.  To add more fonts, put them in your bdf font directory
#                             and update the %qd'font_metric_files array at the bottom of this
#                             file.  It maps a key consisting of the Quickdraw font number, 
#                             font size, and font style (0 for plain, 1 for italic, 2 for bold,
#                             3 for both) to the appropriate bdf file.

# RECTANGLES
#
# Draw rectangles using the routines:
#   &qd'FrameRect($myRect);                     # Draw wire-frame rectangle
#   &qd'PaintRect($myRect);                     # Fill rectangle with current foreground
#                                                 color and pattern
#   &qd'EraseRect($myRect);                     # Erase the rectangle (fill with bg color)
#   &qd'InvertRect($myRect);                    # Invert black and white in rectangle


# OVALS
#
# Draw ovals using the routines:
#   &qd'FrameOval($myRect);                     # Draw wire-frame oval
#   &qd'PaintOval($myRect);                     # Fill oval with current foreground
#                                                 color and pattern
#   &qd'EraseOval($myRect);                     # Erase the oval (fill with bg color)
#   &qd'InvertOval($myRect);                    # Invert black and white in oval
#   &qd'FillOval($myRect,$pat);                 # Fill with specified pattern
# 

# ROUND RECTANGLES
# 
# Draw round-cornered rectangles with these routines.  They each take an oval radius
# to determine the amount of curvature.  Values of 10-20 are typical.
#   &qd'FrameRoundRect($myRect,$ovalWidth,$ovalHeight); # wire-frame outline
#   &qd'PaintRoundRect($myRect,$ovalWidth,$ovalHeight); # fill with current foreground
#   &qd'EraseRoundRect($myRect,$ovalWidth,$ovalHeight); # erase
#   &qd'InvertRoundRect($myRect,$ovalWidth,$ovalHeight);# invert
#   &qd'FillRoundRect($myRect,$ovalWidth,$ovalHeight,$pat); # fill with specified pattern

# ARCS
# Draw an arc subtending the specified rectangle.  Angles are in degrees and
# start pointing northward and get larger clockwise:
# e.g. PaintArc($r,45,90) gives you a pie wedge from 2 o'clock to 5 o'clock
#   &qd'FrameArc($rect,$startAngle,$arcAngle);  # wire-frame the arc
#   &qd'PaintArc($rect,$startAngle,$arcAngle);  # fill with current foreground
#   &qd'EraseArc($rect,$startAngle,$arcAngle);  # erase arc
#   &qd'InvertArc($rect,$startAngle,$arcAngle);  # flip white and black
#   &qd'FillArc($rect,,$startAngle,$arcAngle,$pat);  # fill with specified pattern

# POLYGONS
# Calling OpenPoly returns the name of a variable in which a growing
# polygon structure will be stored.  Once a polygon is opened, all drawing
# commands cease to have an effect on the picture.  Instead, all MoveTo,
# LineTo and Line commands accumulate polygon vertices into the data structure.
# Call ClosePoly to stop recording drawing commands.  The polygon can now
# be moved, scaled, drawn, filled and erased as many times as wished.  Call
# KillPoly to release the memory taken up by the polygon
#   $polygon = &qd'OpenPoly;                      # begin recording drawing commands
#   &qd'ClosePoly($polygon);                      # stop recording drawing commands
#   &qd'FramePoly($polygon);                      # wire-frame the polygon
#   &qd'PaintPoly($polygon);                      # fill with current foreground
#   &qd'ErasePoly($polygon);                      # erase polygon
#   &qd'FillPoly($polygon,$pat);                  # fill polygon with pattern
#   &qd'OffsetPoly($polygon,$dh,$dv);             # translate poly by dh horizontally, dv vertically
#   &qd'MapPoly($polygon,$srcRect,$destRect);     # map polygon from coordinate system defined by
                                                  #  source rectangle to that defined by destination
                                                  #  rectangle (moving or resizing it as needed)

# PRINTING OUT THE PICTURE IN A FORM THAT THE MACINTOSH CAN READ
#
# The Mac expects its picture files to begin with 512 bytes of "application specific"
# data.  By default the picture data that you get will be proceeded by 512 bytes of
# 0's.  If you want something else, or if you just want the picture data, set the
# package variable $qd'PICTHEADER to whatever you desire before calling ClosePicture.
# In order for the picture data to be readable on the Macintosh, the file type must
# be set to 'PICT'.  A number of UNIX utilities, including mcvert and BinHex allow
# you to do this.  Or you can use the picttoppm utility (part of the netppm suite of
# graphics tools) to translate the file into any format you desire.

# A WORKING EXAMPLE
# require "qd.pl";
# &qd'SetRect(*myRect,0,0,500,500);          # Define a 500 pixel square
# &qd'OpenPicture($myRect);                  # Begin defining the picture
# &qd'ClipRect($myRect);                     # Always a good idea
# &qd'MoveTo(5,5);                           # Move the pen to a starting point
# &qd'LineTo(400,400);                       # A diagonal line
# &qd'TextFont($qd'COURIER);                 # Set the font
# &qd'MoveTo(50,20);                         # Move the pen to a new starting point
# &qd'DrawString("Hello there!");            # Friendly greeting
# &qd'SetRect(*myRect,80,80,250,250);        # New rectangle
# &qd'RGBForeColor(0x0000,0x0000,0xFFFF);    # Set the color to blue
# &qd'PaintRect($myRect);                    # Fill rectangle with that color
# $data = &qd'ClosePicture;                  # Close picture and retrieve data

#  # Pipe through binhex, setting the creator type to JVWR for JPEG Viewer
#  # Note: BinHex is available at <ftp://genome.wi.mit.edu/software/util/BinHex>
# open (BINHEX "| BinHex -t PICT -c JVWR -n 'An Example'"); 
# print BINHEX $data;
# close BINHEX;

# # Turn it into a GIF file, using the ppm utilities
# open (GIF, "| picttoppm | ppmtogif -transparent white");
# print GIF $data;
# close GIF;


# MISCELLANEOUS NOTES
# NOTE: For some reason the various FILL routines don't work as
# advertised.  They are simulated by a PnPat followed by a paint

# --------------------------------------------------------------------
# Quickdraw-like functions -- now using PICT2
# --------------------------------------------------------------------
{
package qd;

# Directory to look in to find font metric definitions -- change this
# for your installation
$X11FONTS = '/usr/local/X11R5/X11/fonts/bdf';

# Apple quickdraw constants
$TIMES = 20;
$HELVETICA = 21;
$COURIER = 22;
$SYMBOL = 23;
$NEWCENTURYSCHOOLBK = 34;

$PLAIN = 0;
$BOLD = 1;
$ITALIC = 2;
$UNDERLINE = 4;

# Some minimal patterns -- define your own if you like
$GRAY = pack ('n4',0xAA55,0xAA55,0xAA55,0xAA55);
$DKGRAY = pack ('n4',0xDD77,0xDD77,0xDD77,0xDD77);
$LTGRAY = pack ('n4',0x8822,0x8822,0x8822,0x8822);
$WHITE = pack('n4',0x0000,0x0000,0x0000,0x0000);
$BLACK = pack ('n4',0xFFFF,0xFFFF,0xFFFF,0xFFFF);

# absolute colors to be used with FgColor/BgColor
# (for better control, use RGBFgColor/RGBBgColor)
$BLACKCOLOR = 33;
$WHITECOLOR = 30;
$REDCOLOR = 209;
$GREENCOLOR = 329;
$BLUECOLOR = 389;
$CYANCOLOR = 269;
$MAGENTACOLOR = 149;
$YELLOWCOLOR = 89;

# This defines the header used at the beginning of PICT files:
$PICTHEADER = "\0" x 512;

# These are phoney font metrics which we use when no font metrics files are
# around to help us out.
$fudgefactor = 0.55;
$ITALICEXTRA = 0.05;
$BOLDEXTRA = 0.08;

# Initial starting values
$textFont = $HELVETICA;
$textSize = 12;
$textFace = $PLAIN;
$rgbfgcolor = pack('n*',0xFFFF,0xFFFF,0xFFFF);
$rgbbgcolor = pack('n*',0,0,0);
$fgcolor = $BLACKCOLOR;
$bgcolor = $WHITECOLOR;
$polySave = undef;

$_PnPattern = $BLACK;
$_polyName = "polygon000";

sub OpenPicture {               # begin a picture
    local($rect) = @_;
    $currH = $currV = 0;        # current pen position
    $pict = $PICTHEADER;        # the header
    $pict .= pack('n',0);       # size int (placeholder)
    $pict .= $rect;             # pict frame
    $pict .= pack('n',0x0011);  # Type 2 picture
    $pict .= pack('n',0x02FF);  # version number
    $pict .= pack('nC24',0x0C00,0);     # reserved header opcode + 24 bytes of reserved data
    # initialize the font and size
    &TextFont($textFont);
    &TextSize($textSize);
    &TextFace($textFace);
}

sub ClosePicture {              # close pict and return it
    $pict .= pack ('n',0x00FF); # end of pict code
    substr($pict,512,2) = pack('n',length($pict) - 512); # fill in length 
    return $pict;
}

sub ClipRect {
    local($rect) = @_;
    $pict .= pack('nn',0x0001,0x0A) . $rect;
}

sub PenPat {
    local($newpat) = @_;
    return unless $newpat ne $_PnPattern;
    $_PnPattern = $newpat;
    $pict .= pack('n',0x0009) . $_PnPattern;
}

sub RGBForeColor {
    local($rgb) = pack('n3',@_);
    return unless $rgb ne $rgbfgcolor;
    $rgbfgcolor = $rgb;
    $pict .= pack('n',0x001A) . $rgbfgcolor;
}

sub RGBBackColor {
    local($rgb) = pack('n3',@_);
    return unless $rgb ne $rgbbgcolor;
    $rgbbgcolor = $rgb;
    $pict .= pack('n',0x001B) . $rgbbgcolor;
}

sub FgColor {
    local($color) = @_;
    return unless $color != $fgcolor;
    $fgcolor = $color;
    $pict .= pack('nL',0x000E,$color);
}

sub BgColor {
    local($color) = @_;
    return unless $color != $bgcolor;
    $bgcolor = $color;
    $pict .= pack('nL',0x000F,$color);
}

sub TextFont {
    local($font) = @_;
    $textFont = $font;
    $pict .= pack('nn',0x0003,$font);
}

sub TextSize {
    local($size) = @_;
    $textSize = $size;
    $pict .= pack('nn',0x000D,$size);
}

sub PenSize {
    local($h,$v) = @_;
    $pict .= pack('nnn',0x0007,$v,$h);
}

sub TextFace {
    return if $textFace == @_[0];
    $textFace = @_[0];
    $pict .= pack ('nCC',0x0004,$textFace,0); # (zero added to pad to word)
}

sub DrawString {
    local($text) = @_;
    $text .= "\0" x ((length($text) + 1) % 2); # pad text to an odd length
    $pict .= pack('nnnC',0x0028,$currV,$currH,length($text)) . $text;
}

# RECTANGLE MANIPULATION ROUTINES.  Note that
# the rectangles are passed by NAME rather than by value,
# in accordance with the MacOS way of doing things.
sub SetRect {
    local(*r,$h1,$v1,$h2,$v2) = @_;
    $r = pack ('n4',$v1,$h1,$v2,$h2);
}

sub OffsetRect {
    local(*r,$x,$y) = @_;
    local($v1,$h1,$v2,$h2) = unpack('n4',$r);
    $h1 += $x; $h2 += $x;
    $v1 += $y; $v2 += $y;
    $r = pack ('n4',$v1,$h1,$v2,$h2);    
}

sub InsetRect {
    local(*r,$x,$y) = @_;
    local($v1,$h1,$v2,$h2) = unpack('n4',$r);
    $h1 -= int($x/2); $h2 -= int($x/2);
    $v1 -= int($y/2); $v2 -= int($y/2);
    $r = pack ('n4',$v1,$h1,$v2,$h2);    
}

# A few utility routine to translate between perl
# arrays and rectangles.

# four-element perl array to quickdraw rect structure
sub a2r {
    local($top,$left,$bottom,$right) = @_;
    return pack('n4',$top,$left,$bottom,$right);
}

# rectangle to four-element perl array
sub r2a {
    local($rect) = @_;
    return unpack('n4',$rect);
}

# associative array in which the keys are 'top','left','bottom','right'
# to quickdraw rect structure
sub aa2r {
    local(%r) = @_;
    return pack('n4',$r{'top'},$r{'left'},$r{'bottom'},$r{'right'});
}

# quickdraw rect structure to associative array
sub r2aa {
    local($r) = @_;
    local(%r);
    ($r{'top'},$r{'left'},$r{'bottom'},$r{'right'}) = unpack('n4',$r);
    return %r;
}

# LINE DRAWING ROUTINES
sub MoveTo {
    ($currH,$currV) = @_;
}

sub Move {
    local($dh,$dv) = @_;
    $currH += $dh;
    $currV += $dv;
}

sub LineTo {
    local($h,$v) = @_;
    # Special handling for polygons
    if (defined(@polySave)) {
        &_addVertex(*polySave,$h,$v)
    } else {
        $pict .= pack('nn4',0x0020,$currV,$currH,$v,$h);
    }
    ($currH,$currV) = ($h,$v);
}

sub Line {
    local($dh,$dv) = @_;
    # Special handling for polygons
    if (defined(@polySave)) {
        &_addVertex(*polySave,$h,$v);
    } else {
        $pict .= pack('nn4',0x0020,$currV,$currH,$currV+$dv,$currH+$dh);
    }
    ($currH,$currV) = ($currH+$dh,$currV+$dv);
}

sub Scale { #use picComment to set laserwriter line scaling
    local($numerator,$denominator)= @_;
    $pict .= pack('nnnn2',0x00A1,182,4,$numerator,$denominator);
}


# Rectangles
sub FrameRect {
    local($rect) = @_;
    $pict .= pack('n',0x0030) . $rect;
}

sub PaintRect {
    local($rect) = @_;
    $pict .= pack('n',0x0031) . $rect;
}

sub EraseRect {
    local($rect) = @_;
    $pict .= pack('n',0x0032) . $rect;
}

sub InvertRect {
    local($rect) = @_;
    $pict .= pack('n',0x0033) . $rect;
}

sub FillRect {
    local($rect,$pattern) = @_;
    local($oldpat) = $_PnPattern;
    &PenPat($pattern);
    &PaintRect($rect);
    &PenPat($oldpat);
}

# Ovals
sub FrameOval {
    local($rect) = @_;
    $pict .= pack('n',0x0050) . $rect;
}

sub PaintOval {
    local($rect) = @_;
    $pict .= pack('n',0x0051) . $rect;
}

sub EraseOval {
    local($rect) = @_;
    $pict .= pack('n',0x0052) . $rect;
}

sub InvertOval {
    local($rect) = @_;
    $pict .= pack('n',0x0053) . $rect;
}

sub FillOval {
    local($rect,$pattern) = @_;
    local($oldpat) = $_PnPattern;
    &PenPat($pattern);
    &PaintOval($rect);
    &PenPat($oldpat);
}

# Arcs
sub FrameArc {
    local($rect,$startAngle,$arcAngle) = @_;
    $pict .= pack('n',0x0060) . $rect;
    $pict .= pack('nn',$startAngle,$arcAngle);
}

sub PaintArc {
    local($rect,$startAngle,$arcAngle) = @_;
    $pict .= pack('n',0x0061) . $rect;
    $pict .= pack('nn',$startAngle,$arcAngle);
}

sub EraseArc {
    local($rect,$startAngle,$arcAngle) = @_;
    $pict .= pack('n',0x0062) . $rect;
    $pict .= pack('nn',$startAngle,$arcAngle);
}

sub InvertArc {
    local($rect,$startAngle,$arcAngle) = @_;
    $pict .= pack('n',0x0063) . $rect;
    $pict .= pack('nn',$startAngle,$arcAngle);
}

sub FillArc {
    local($rect,$startAngle,$arcAngle,$pattern) = @_;
    local($oldpat) = $_PnPattern;
    &PenPat($pattern);
    &PaintArc($rect,$startAngle,$arcAngle);
    &PenPat($oldpat);
}

# Round rects
sub FrameRoundRect {
    local($rect,$ovalWidth,$ovalHeight) = @_;
    unless ($_roundRectCurvature eq "$ovalWidth $ovalHeight") {
        $pict .= pack('nn2',0x000B,$ovalHeight,$ovalWidth);
        $_roundRectCurvature = "$ovalWidth $ovalHeight";
    }
    $pict .= pack('n',0x0040) . $rect;
}

sub PaintRoundRect {
    local($rect,$ovalWidth,$ovalHeight) = @_;
    unless ($_roundRectCurvature eq "$ovalWidth $ovalHeight") {
        $pict .= pack('nn2',0x000B,$ovalHeight,$ovalWidth);
        $_roundRectCurvature = "$ovalWidth $ovalHeight";
    }
    $pict .= pack('n',0x0041) . $rect;
}

sub EraseRoundRect {
    local($rect,$ovalWidth,$ovalHeight) = @_;
    unless ($_roundRectCurvature eq "$ovalWidth $ovalHeight") {
        $pict .= pack('nn2',0x000B,$ovalHeight,$ovalWidth);
        $_roundRectCurvature = "$ovalWidth $ovalHeight";
    }
    $pict .= pack('n',0x0042) . $rect;
}

sub InvertRoundRect {
    local($rect,$ovalWidth,$ovalHeight) = @_;
    unless ($_roundRectCurvature eq "$ovalWidth $ovalHeight") {
        $pict .= pack('nn2',0x000B,$ovalHeight,$ovalWidth);
        $_roundRectCurvature = "$ovalWidth $ovalHeight";
    }
    $pict .= pack('n',0x0043) . $rect;
}

sub FillRoundRect {
    local($rect,$ovalWidth,$ovalHeight,$pattern) = @_;
    local($oldpat) = $_PnPattern;
    &PenPat($pattern);
    &PaintRoundRect($rect,$ovalWidth,$ovalHeight);
    &PenPat($oldpat);
}

# Polygons -- you are only allowed to create one polygon at a time.
# You will be returned a "handle" which contains the growing polygon
# structure.  The "handle" is actually the NAME of the scalar
sub OpenPoly {
    $_polyName++;
    undef $polySave;            # close one if it was already defined
    *polySave = $_polyName;
    @polySave = (10,0,0,0,0); # initialize it to empty size and rectangle
    return $_polyName;
}
 
sub ClosePoly {
    *polySave = 'scratch';
    undef @polySave;
}

# Kill the poly -- really a no-op in perl
sub KillPoly {
    local(*poly) = @_;
    undef @poly;
}

# Polygon drawing
sub FramePoly {
    local(*poly) = @_;
    return unless @poly;
    $pict .= pack('n*',0x0070,@poly);
}

sub PaintPoly {
    local(*poly) = @_;
    return unless @poly;
    $pict .= pack('n*',0x0071,@poly);
}

sub ErasePoly {
    local(*poly) = @_;
    return unless @poly;
    $pict .= pack('n*',0x0072,@poly);
}

sub InvertPoly {
    local(*poly) = @_;
    return unless @poly;
    $pict .= pack('n*',0x0073,@poly);
}

sub FillPoly {
    local(*poly,$pattern) = @_;
    return unless @poly;
    local($oldpat) = $_PnPattern;
    &PenPat($pattern);
    &PaintPoly(*poly);
    &PenPat($oldpat);
}

sub OffsetPoly {
    local(*poly,$dh,$dv) = @_; 
  return unless @poly;
    local($size,@vertices) = @poly;
    local($i);
    for ($i=0;$i<@vertices;$i+=2) {
        $vertices[$i] += $dv;
        $vertices[$i+1] += $dh;
    }
    @poly = ($size,@vertices);
}

sub MapPoly {
    local(*poly,$srcRect,$destRect) = @_;
    return unless @poly;
    local($size,@vertices) = @poly;
    local(@src) = unpack('n4',$srcRect);
    local(@dest) = unpack('n4',$destRect);
    local($factorV) = ($dest[2]-$dest[0])/($src[2]-$src[0]);
    local($factorH) = ($dest[3]-$dest[1])/($src[3]-$src[1]);
    for ($i=0;$i<@vertices;$i+=2) {
        $vertices[$i] = int($dest[0] + ($vertices[$i] - $src[0]) * $factorV);
        $vertices[$i+1] = int($dest[1] + ($vertices[$i+1] - $src[1]) * $factorH);
    }
    @poly = ($size,@vertices);
}

# A utility routine to add a vertex to the growing polygon structure
# We need to grow both the size of the polygon and increase the bounding
# rectangle.  A special case occurs when we add the first vertex:
# we store both the current position 
sub _addVertex {
    local(*polygon,$h,$v) = @_;
    local($size,$top,$left,$bottom,$right,@vertices) = @polygon;
    # Special case for empty vertices -- add the current point
    unless (@vertices) {
        push(@vertices,$currV,$currH);
        $size += 4;
        $top = $bottom = $currV;
        $left = $right = $currH;
    }

    # IM V1 implies that all vertices are stored relative to
    # the first point -- I don't know if this is really the case
    push (@vertices,$v,$h);

    $size += 4;
    $top = $v if $v < $top;
    $bottom = $v if $v > $bottom;
    $left = $h if $h < $left;
    $right = $h if $h > $right;
    @polygon=($size,$top,$left,$bottom,$right,@vertices);
}

# We try to get the metrics from an X11 bdf font file, if possible.
sub TextWidth {
    local($text) = @_;

    # See if we can derive the character widths from a metrics file
    local($face) = 0xFB & $textFace; # underlining don't count
    local($metric_name) = &_getFontMetrics($textFont,$textSize,$face);
    if ($metric_name && (*metrics = $metric_name) && defined(%metrics)) {
        local($length);
        foreach (split('',$text)) {
            $length += $metrics{ord($_)};
        }
        return $length;
    } else {                    # we get here if we don't have any metrics - make it up
        local($extra);
        $extra += $ITALICEXTRA if vec($textFace,$ITALIC,1);
        $extra += $BOLDEXTRA if vec($textFace,$BOLD,1);
        return length($text) * $textSize * ($fudgefactor+$extra);
    }
}

# Utility routine to read text widths out of bdf files.  We create a metrics
# array on the fly.  The names of the metrics files are stored in an array
# called _metricsArrays.  We return the name of the array, or undef if inapplicable.
sub _getFontMetrics {
    local($font,$size,$face) = @_;
    local($key) = "$font $size $face";
    return $_metricsArrays{$key} if $_metricsArrays{$key};

    # If we get here, we don't have a metrics array to return.  See if we can
    # construct one from a bdf file.

    # Don't bother unless this font is defined.
    return undef unless $font_metric_files{$key};

    # Don't bother if we tried before and failed
    return undef if $_failed_metric{$key};

    # Try to open up the bdf file.  Remember if we fail
    unless (open(BDF,"$font_metric_files{$key}")) {
        $_failed_metric_files{$key}++;
        return undef;
    }

    # Wow! We're golden.  Create a new metrics array
    $next_metric++;             # bump up the name
    local(*metrics) = $next_metric; local($char);
    while (<BDF>) {
        next unless /^STARTCHAR/../^ENDCHAR/;
        if (/^ENCODING\s+(\d+)/) { $char = $1; }
        elsif (/^DWIDTH\s+(\d+)/)   { $metrics{$char}=$1; }
    }
    close(BDF);
    
    # Remember the name of the metrics array and return it
    return $_metricsArrays{$key} = $next_metric;
}

# Ugly stuff that I want to hide at the bottom

# For the purposes of mapping from quickdraw fonts to X11fonts, we define
# the following dictionary:
%font_metric_files = (
                      "22 8 1","$X11FONTS/courB08.bdf",
                      "22 10 1","$X11FONTS/courB10.bdf",
                      "22 12 1","$X11FONTS/courB12.bdf",
                      "22 14 1","$X11FONTS/courB14.bdf",
                      "22 18 1","$X11FONTS/courB18.bdf",
                      "22 24 1","$X11FONTS/courB24.bdf",
                      "22 8 2","$X11FONTS/courO08.bdf",
                      "22 10 2","$X11FONTS/courO10.bdf",
                      "22 12 2","$X11FONTS/courO12.bdf",
                      "22 14 2","$X11FONTS/courO14.bdf",
                      "22 18 2","$X11FONTS/courO18.bdf",
                      "22 24 2","$X11FONTS/courO24.bdf",
                      "22 8 0","$X11FONTS/courR08.bdf",
                      "22 10 0","$X11FONTS/courR10.bdf",
                      "22 12 0","$X11FONTS/courR12.bdf",
                      "22 14 0","$X11FONTS/courR14.bdf",
                      "22 18 0","$X11FONTS/courR18.bdf",
                      "22 24 0","$X11FONTS/courR24.bdf",
                      "21 8 1","$X11FONTS/helvB08.bdf",
                      "21 10 1","$X11FONTS/helvB10.bdf",
                      "21 12 1","$X11FONTS/helvB12.bdf",
                      "21 14 1","$X11FONTS/helvB14.bdf",
                      "21 18 1","$X11FONTS/helvB18.bdf",
                      "21 24 1","$X11FONTS/helvB24.bdf",
                      "21 8 2","$X11FONTS/helvO08.bdf",
                      "21 10 2","$X11FONTS/helvO10.bdf",
                      "21 12 2","$X11FONTS/helvO12.bdf",
                      "21 14 2","$X11FONTS/helvO14.bdf",
                      "21 18 2","$X11FONTS/helvO18.bdf",
                      "21 24 2","$X11FONTS/helvO24.bdf",
                      "21 8 0","$X11FONTS/helvR08.bdf",
                      "21 10 0","$X11FONTS/helvR10.bdf",
                      "21 12 0","$X11FONTS/helvR12.bdf",
                      "21 14 0","$X11FONTS/helvR14.bdf",
                      "21 18 0","$X11FONTS/helvR18.bdf",
                      "21 24 0","$X11FONTS/helvR24.bdf",
                      "20 8 1","$X11FONTS/timB08.bdf",
                      "20 10 1","$X11FONTS/timB10.bdf",
                      "20 12 1","$X11FONTS/timB12.bdf",
                      "20 14 1","$X11FONTS/timB14.bdf",
                      "20 18 1","$X11FONTS/timB18.bdf",
                      "20 24 1","$X11FONTS/timB24.bdf",
                      "20 8 3","$X11FONTS/timBI08.bdf",
                      "20 10 3","$X11FONTS/timBI10.bdf",
                      "20 12 3","$X11FONTS/timBI12.bdf",
                      "20 14 3","$X11FONTS/timBI14.bdf",
                      "20 18 3","$X11FONTS/timBI18.bdf",
                      "20 24 3","$X11FONTS/timBI24.bdf",
                      "20 8 2","$X11FONTS/timI08.bdf",
                      "20 10 2","$X11FONTS/timI10.bdf",
                      "20 12 2","$X11FONTS/timI12.bdf",
                      "20 14 2","$X11FONTS/timI14.bdf",
                      "20 18 2","$X11FONTS/timI18.bdf",
                      "20 24 2","$X11FONTS/timI24.bdf",
                      "20 8 0","$X11FONTS/timR08.bdf",
                      "20 10 0","$X11FONTS/timR10.bdf",
                      "20 12 0","$X11FONTS/timR12.bdf",
                      "20 14 0","$X11FONTS/timR14.bdf",
                      "20 18 0","$X11FONTS/timR18.bdf",
                      "20 24 0","$X11FONTS/timR24.bdf",
                      "34 8 1","$X11FONTS/ncenB08.bdf",
                      "34 10 1","$X11FONTS/ncenB10.bdf",
                      "34 12 1","$X11FONTS/ncenB12.bdf",
                      "34 14 1","$X11FONTS/ncenB14.bdf",
                      "34 18 1","$X11FONTS/ncenB18.bdf",
                      "34 24 1","$X11FONTS/ncenB24.bdf",
                      "34 8 3","$X11FONTS/ncenBI08.bdf",
                      "34 10 3","$X11FONTS/ncenBI10.bdf",
                      "34 12 3","$X11FONTS/ncenBI12.bdf",
                      "34 14 3","$X11FONTS/ncenBI14.bdf",
                      "34 18 3","$X11FONTS/ncenBI18.bdf",
                      "34 24 3","$X11FONTS/ncenBI24.bdf",
                      "34 8 2","$X11FONTS/ncenI08.bdf",
                      "34 10 2","$X11FONTS/ncenI10.bdf",
                      "34 12 2","$X11FONTS/ncenI12.bdf",
                      "34 14 2","$X11FONTS/ncenI14.bdf",
                      "34 18 2","$X11FONTS/ncenI18.bdf",
                      "34 24 2","$X11FONTS/ncenI24.bdf",
                      "34 8 0","$X11FONTS/ncenR08.bdf",
                      "34 10 0","$X11FONTS/ncenR10.bdf",
                      "34 12 0","$X11FONTS/ncenR12.bdf",
                      "34 14 0","$X11FONTS/ncenR14.bdf",
                      "34 18 0","$X11FONTS/ncenR18.bdf",
                      "34 24 0","$X11FONTS/ncenR24.bdf"
                      );
$next_metric = "metrics0000";   # name of our metrics arrays - dynamically allocated

1;
}       #end of package qd

