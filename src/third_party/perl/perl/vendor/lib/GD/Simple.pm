package GD::Simple;

=head1 NAME

GD::Simple - Simplified interface to GD library

=head1 SYNOPSIS

For a nice tutorial on using this module, see Gabor Szabo's article at
http://perlmaven.com/drawing-images-using-gd-simple.

    use GD::Simple;

    # create a new image
    $img = GD::Simple->new(400,250);

    # draw a red rectangle with blue borders
    $img->bgcolor('red');
    $img->fgcolor('blue');
    $img->rectangle(10,10,50,50);

    # draw an empty rectangle with green borders
    $img->bgcolor(undef);
    $img->fgcolor('green');
    $img->rectangle(30,30,100,100);

    # move to (80,80) and draw a green line to (100,190)
    $img->moveTo(80,80);
    $img->lineTo(100,190);

    # draw a solid orange ellipse
    $img->moveTo(110,100);
    $img->bgcolor('orange');
    $img->fgcolor('orange');
    $img->ellipse(40,40);

    # draw a black filled arc
    $img->moveTo(150,150);
    $img->fgcolor('black');
    $img->arc(50,50,0,100,gdNoFill|gdEdged);

    # draw a string at (10,180) using the default
    # built-in font
    $img->moveTo(10,180);
    $img->string('This is very simple');

    # draw a string at (280,210) using 20 point
    # times italic, angled upward 90 degrees
    $img->moveTo(280,210);
    $img->font('Times:italic');
    $img->fontsize(20);
    $img->angle(-90);
    $img->string('This is very fancy');

    # some turtle graphics
    $img->moveTo(300,100);
    $img->penSize(3,3);
    $img->angle(0);
    $img->line(20);   # 20 pixels going to the right
    $img->turn(30);   # set turning angle to 30 degrees
    $img->line(20);   # 20 pixel line
    $img->line(20);
    $img->line(20);
    $img->turn(-90); # set turning angle to -90 degrees
    $img->line(50);  # 50 pixel line

    # draw a cyan polygon edged in blue
    my $poly = new GD::Polygon;
    $poly->addPt(150,100);
    $poly->addPt(199,199);
    $poly->addPt(100,199);
    $img->bgcolor('cyan');
    $img->fgcolor('blue');
    $img->penSize(1,1);
    $img->polygon($poly);

   # convert into png data
   print $img->png;

=head1 DESCRIPTION

GD::Simple is a subclass of the GD library that shortens many of the
long GD method calls by storing information about the pen color, size
and position in the GD object itself.  It also adds a small number of
"turtle graphics" style calls for those who prefer to work in polar
coordinates.  In addition, the library allows you to use symbolic
names for colors, such as "chartreuse", and will manage the colors for
you.

=head2 The Pen

GD::Simple maintains a "pen" whose settings are used for line- and
shape-drawing operations.  The pen has the following properties:

=over 4

=item fgcolor

The pen foreground color is the color of lines and the borders of
filled and unfilled shapes.

=item bgcolor

The pen background color is the color of the contents of filled
shapes.

=item pensize

The pen size is the width of the pen.  Larger sizes draw thicker
lines.

=item position

The pen position is its current position on the canvas in (X,Y)
coordinates.

=item angle

When drawing in turtle mode, the pen angle determines the current
direction of lines of relative length.

=item turn

When drawing in turtle mode, the turn determines the clockwise or
counterclockwise angle that the pen will turn before drawing the next
line.

=item font

The font to use when drawing text.  Both built-in bitmapped fonts and
TrueType fonts are supported.

=item fontsize

The size of the font to use when drawing with TrueType fonts.

=back

One sets the position and properties of the pen and then draws.  As
the drawing progresses, the position of the pen is updated.

=head2 Methods

GD::Simple introduces a number of new methods, a few of which have the
same name as GD::Image methods, and hence change their behavior. In
addition to these new methods, GD::Simple objects support all of the
GD::Image methods. If you make a method call that isn't directly
supported by GD::Simple, it refers the request to the underlying
GD::Image object.  Hence one can load a JPEG image into GD::Simple and
declare it to be TrueColor by using this call, which is effectively
inherited from GD::Image:

  my $img = GD::Simple->newFromJpeg('./myimage.jpg',1);

The rest of this section describes GD::Simple-specific methods.

=cut

use strict;
use GD;
use GD::Group;
use Math::Trig;
use Carp 'croak';

our @ISA = 'Exporter';
our @EXPORT    = @GD::EXPORT;
our @EXPORT_OK = @GD::EXPORT_OK;
our $AUTOLOAD;

my $IMAGECLASS = 'GD::Image';
my $TRANSPARENT;
my %COLORS = (
  white                 => [  0xFF,  0xFF,  0xFF ],
  black                 => [  0x00,  0x00,  0x00 ],
  aliceblue             => [  0xF0,  0xF8,  0xFF ],
  antiquewhite          => [  0xFA,  0xEB,  0xD7 ],
  aqua                  => [  0x00,  0xFF,  0xFF ],
  aquamarine            => [  0x7F,  0xFF,  0xD4 ],
  azure                 => [  0xF0,  0xFF,  0xFF ],
  beige                 => [  0xF5,  0xF5,  0xDC ],
  bisque                => [  0xFF,  0xE4,  0xC4 ],
  blanchedalmond        => [  0xFF,  0xEB,  0xCD ],
  blue                  => [  0x00,  0x00,  0xFF ],
  blueviolet            => [  0x8A,  0x2B,  0xE2 ],
  brown                 => [  0xA5,  0x2A,  0x2A ],
  burlywood             => [  0xDE,  0xB8,  0x87 ],
  cadetblue             => [  0x5F,  0x9E,  0xA0 ],
  chartreuse            => [  0x7F,  0xFF,  0x00 ],
  chocolate             => [  0xD2,  0x69,  0x1E ],
  coral                 => [  0xFF,  0x7F,  0x50 ],
  cornflowerblue        => [  0x64,  0x95,  0xED ],
  cornsilk              => [  0xFF,  0xF8,  0xDC ],
  crimson               => [  0xDC,  0x14,  0x3C ],
  cyan                  => [  0x00,  0xFF,  0xFF ],
  darkblue              => [  0x00,  0x00,  0x8B ],
  darkcyan              => [  0x00,  0x8B,  0x8B ],
  darkgoldenrod         => [  0xB8,  0x86,  0x0B ],
  darkgray              => [  0xA9,  0xA9,  0xA9 ],
  darkgreen             => [  0x00,  0x64,  0x00 ],
  darkkhaki             => [  0xBD,  0xB7,  0x6B ],
  darkmagenta           => [  0x8B,  0x00,  0x8B ],
  darkolivegreen        => [  0x55,  0x6B,  0x2F ],
  darkorange            => [  0xFF,  0x8C,  0x00 ],
  darkorchid            => [  0x99,  0x32,  0xCC ],
  darkred               => [  0x8B,  0x00,  0x00 ],
  darksalmon            => [  0xE9,  0x96,  0x7A ],
  darkseagreen          => [  0x8F,  0xBC,  0x8F ],
  darkslateblue         => [  0x48,  0x3D,  0x8B ],
  darkslategray         => [  0x2F,  0x4F,  0x4F ],
  darkturquoise         => [  0x00,  0xCE,  0xD1 ],
  darkviolet            => [  0x94,  0x00,  0xD3 ],
  deeppink              => [  0xFF,  0x14, 0x100 ],
  deepskyblue           => [  0x00,  0xBF,  0xFF ],
  dimgray               => [  0x69,  0x69,  0x69 ],
  dodgerblue            => [  0x1E,  0x90,  0xFF ],
  firebrick             => [  0xB2,  0x22,  0x22 ],
  floralwhite           => [  0xFF,  0xFA,  0xF0 ],
  forestgreen           => [  0x22,  0x8B,  0x22 ],
  fuchsia               => [  0xFF,  0x00,  0xFF ],
  gainsboro             => [  0xDC,  0xDC,  0xDC ],
  ghostwhite            => [  0xF8,  0xF8,  0xFF ],
  gold                  => [  0xFF,  0xD7,  0x00 ],
  goldenrod             => [  0xDA,  0xA5,  0x20 ],
  gray                  => [  0x80,  0x80,  0x80 ],
  green                 => [  0x00,  0x80,  0x00 ],
  greenyellow           => [  0xAD,  0xFF,  0x2F ],
  honeydew              => [  0xF0,  0xFF,  0xF0 ],
  hotpink               => [  0xFF,  0x69,  0xB4 ],
  indianred             => [  0xCD,  0x5C,  0x5C ],
  indigo                => [  0x4B,  0x00,  0x82 ],
  ivory                 => [  0xFF,  0xFF,  0xF0 ],
  khaki                 => [  0xF0,  0xE6,  0x8C ],
  lavender              => [  0xE6,  0xE6,  0xFA ],
  lavenderblush         => [  0xFF,  0xF0,  0xF5 ],
  lawngreen             => [  0x7C,  0xFC,  0x00 ],
  lemonchiffon          => [  0xFF,  0xFA,  0xCD ],
  lightblue             => [  0xAD,  0xD8,  0xE6 ],
  lightcoral            => [  0xF0,  0x80,  0x80 ],
  lightcyan             => [  0xE0,  0xFF,  0xFF ],
  lightgoldenrodyellow  => [  0xFA,  0xFA,  0xD2 ],
  lightgreen            => [  0x90,  0xEE,  0x90 ],
  lightgrey             => [  0xD3,  0xD3,  0xD3 ],
  lightpink             => [  0xFF,  0xB6,  0xC1 ],
  lightsalmon           => [  0xFF,  0xA0,  0x7A ],
  lightseagreen         => [  0x20,  0xB2,  0xAA ],
  lightskyblue          => [  0x87,  0xCE,  0xFA ],
  lightslategray        => [  0x77,  0x88,  0x99 ],
  lightsteelblue        => [  0xB0,  0xC4,  0xDE ],
  lightyellow           => [  0xFF,  0xFF,  0xE0 ],
  lime                  => [  0x00,  0xFF,  0x00 ],
  limegreen             => [  0x32,  0xCD,  0x32 ],
  linen                 => [  0xFA,  0xF0,  0xE6 ],
  magenta               => [  0xFF,  0x00,  0xFF ],
  maroon                => [  0x80,  0x00,  0x00 ],
  mediumaquamarine      => [  0x66,  0xCD,  0xAA ],
  mediumblue            => [  0x00,  0x00,  0xCD ],
  mediumorchid          => [  0xBA,  0x55,  0xD3 ],
  mediumpurple          => [ 0x100,  0x70,  0xDB ],
  mediumseagreen        => [  0x3C,  0xB3,  0x71 ],
  mediumslateblue       => [  0x7B,  0x68,  0xEE ],
  mediumspringgreen     => [  0x00,  0xFA,  0x9A ],
  mediumturquoise       => [  0x48,  0xD1,  0xCC ],
  mediumvioletred       => [  0xC7,  0x15,  0x85 ],
  midnightblue          => [  0x19,  0x19,  0x70 ],
  mintcream             => [  0xF5,  0xFF,  0xFA ],
  mistyrose             => [  0xFF,  0xE4,  0xE1 ],
  moccasin              => [  0xFF,  0xE4,  0xB5 ],
  navajowhite           => [  0xFF,  0xDE,  0xAD ],
  navy                  => [  0x00,  0x00,  0x80 ],
  oldlace               => [  0xFD,  0xF5,  0xE6 ],
  olive                 => [  0x80,  0x80,  0x00 ],
  olivedrab             => [  0x6B,  0x8E,  0x23 ],
  orange                => [  0xFF,  0xA5,  0x00 ],
  orangered             => [  0xFF,  0x45,  0x00 ],
  orchid                => [  0xDA,  0x70,  0xD6 ],
  palegoldenrod         => [  0xEE,  0xE8,  0xAA ],
  palegreen             => [  0x98,  0xFB,  0x98 ],
  paleturquoise         => [  0xAF,  0xEE,  0xEE ],
  palevioletred         => [  0xDB,  0x70, 0x100 ],
  papayawhip            => [  0xFF,  0xEF,  0xD5 ],
  peachpuff             => [  0xFF,  0xDA,  0xB9 ],
  peru                  => [  0xCD,  0x85,  0x3F ],
  pink                  => [  0xFF,  0xC0,  0xCB ],
  plum                  => [  0xDD,  0xA0,  0xDD ],
  powderblue            => [  0xB0,  0xE0,  0xE6 ],
  purple                => [  0x80,  0x00,  0x80 ],
  red                   => [  0xFF,  0x00,  0x00 ],
  rosybrown             => [  0xBC,  0x8F,  0x8F ],
  royalblue             => [  0x41,  0x69,  0xE1 ],
  saddlebrown           => [  0x8B,  0x45,  0x13 ],
  salmon                => [  0xFA,  0x80,  0x72 ],
  sandybrown            => [  0xF4,  0xA4,  0x60 ],
  seagreen              => [  0x2E,  0x8B,  0x57 ],
  seashell              => [  0xFF,  0xF5,  0xEE ],
  sienna                => [  0xA0,  0x52,  0x2D ],
  silver                => [  0xC0,  0xC0,  0xC0 ],
  skyblue               => [  0x87,  0xCE,  0xEB ],
  slateblue             => [  0x6A,  0x5A,  0xCD ],
  slategray             => [  0x70,  0x80,  0x90 ],
  snow                  => [  0xFF,  0xFA,  0xFA ],
  springgreen           => [  0x00,  0xFF,  0x7F ],
  steelblue             => [  0x46,  0x82,  0xB4 ],
  tan                   => [  0xD2,  0xB4,  0x8C ],
  teal                  => [  0x00,  0x80,  0x80 ],
  thistle               => [  0xD8,  0xBF,  0xD8 ],
  tomato                => [  0xFF,  0x63,  0x47 ],
  turquoise             => [  0x40,  0xE0,  0xD0 ],
  violet                => [  0xEE,  0x82,  0xEE ],
  wheat                 => [  0xF5,  0xDE,  0xB3 ],
  whitesmoke            => [  0xF5,  0xF5,  0xF5 ],
  yellow                => [  0xFF,  0xFF,  0x00 ],
  yellowgreen           => [  0x9A,  0xCD,  0x32 ],
  gradient1             => [  0x00,  0xFF,  0x00 ],
  gradient2             => [  0x0A,  0xFF,  0x00 ],
  gradient3             => [  0x14,  0xFF,  0x00 ],
  gradient4             => [  0x1E,  0xFF,  0x00 ],
  gradient5             => [  0x28,  0xFF,  0x00 ],
  gradient6             => [  0x32,  0xFF,  0x00 ],
  gradient7             => [  0x3D,  0xFF,  0x00 ],
  gradient8             => [  0x47,  0xFF,  0x00 ],
  gradient9             => [  0x51,  0xFF,  0x00 ],
  gradient10            => [  0x5B,  0xFF,  0x00 ],
  gradient11            => [  0x65,  0xFF,  0x00 ],
  gradient12            => [  0x70,  0xFF,  0x00 ],
  gradient13            => [  0x7A,  0xFF,  0x00 ],
  gradient14            => [  0x84,  0xFF,  0x00 ],
  gradient15            => [  0x8E,  0xFF,  0x00 ],
  gradient16            => [  0x99,  0xFF,  0x00 ],
  gradient17            => [  0xA3,  0xFF,  0x00 ],
  gradient18            => [  0xAD,  0xFF,  0x00 ],
  gradient19            => [  0xB7,  0xFF,  0x00 ],
  gradient20            => [  0xC1,  0xFF,  0x00 ],
  gradient21            => [  0xCC,  0xFF,  0x00 ],
  gradient22            => [  0xD6,  0xFF,  0x00 ],
  gradient23            => [  0xE0,  0xFF,  0x00 ],
  gradient24            => [  0xEA,  0xFF,  0x00 ],
  gradient25            => [  0xF4,  0xFF,  0x00 ],
  gradient26            => [  0xFF,  0xFF,  0x00 ],
  gradient27            => [  0xFF,  0xF4,  0x00 ],
  gradient28            => [  0xFF,  0xEA,  0x00 ],
  gradient29            => [  0xFF,  0xE0,  0x00 ],
  gradient30            => [  0xFF,  0xD6,  0x00 ],
  gradient31            => [  0xFF,  0xCC,  0x00 ],
  gradient32            => [  0xFF,  0xC1,  0x00 ],
  gradient33            => [  0xFF,  0xB7,  0x00 ],
  gradient34            => [  0xFF,  0xAD,  0x00 ],
  gradient35            => [  0xFF,  0xA3,  0x00 ],
  gradient36            => [  0xFF,  0x99,  0x00 ],
  gradient37            => [  0xFF,  0x8E,  0x00 ],
  gradient38            => [  0xFF,  0x84,  0x00 ],
  gradient39            => [  0xFF,  0x7A,  0x00 ],
  gradient40            => [  0xFF,  0x70,  0x00 ],
  gradient41            => [  0xFF,  0x65,  0x00 ],
  gradient42            => [  0xFF,  0x5B,  0x00 ],
  gradient43            => [  0xFF,  0x51,  0x00 ],
  gradient44            => [  0xFF,  0x47,  0x00 ],
  gradient45            => [  0xFF,  0x3D,  0x00 ],
  gradient46            => [  0xFF,  0x32,  0x00 ],
  gradient47            => [  0xFF,  0x28,  0x00 ],
  gradient48            => [  0xFF,  0x1E,  0x00 ],
  gradient49            => [  0xFF,  0x14,  0x00 ],
  gradient50            => [  0xFF,  0x0A,  0x00 ],
);

sub AUTOLOAD {
  my $self = shift;
  my($pack,$func_name) = $AUTOLOAD=~/(.+)::([^:]+)$/;
  return if $func_name eq 'DESTROY';

  if (ref $self && exists $self->{gd}) {
    $self->{gd}->$func_name(@_);
  } else {
    my @result = $IMAGECLASS->$func_name(@_);
    if (UNIVERSAL::isa($result[0],'GD::Image')) {
      return $self->new($result[0]);
    } else {
      return @result;
    }
  }
}

=over 4

=item $img = GD::Simple->new($x,$y [,$truecolor])

=item $img = GD::Simple->new($gd)

Create a new GD::Simple object. There are two forms of new(). In the
first form, pass the width and height of the desired canvas, and
optionally a boolean flag to request a truecolor image. In the second
form, pass a previously-created GD::Image object.

=cut

# dual-purpose code - beware
sub new {
  my $pack = shift;

  unshift @_,(100,100) if @_ == 0;

  if (@_ >= 2) { # traditional GD::Image->new() call
    my $gd   = $IMAGECLASS->new(@_);
    my $self = $pack->new($gd);
    $self->clear;
    return $self;
  }

  if (@_ == 1) { # initialize from existing image
    my $gd   = shift;
    my $self = bless {
		      gd             => $gd,
		      xy             => [0,0],
		      font           => gdSmallFont,
		      fontsize       => 9,
		      turningangle   => 0,
		      angle          => 0,
		      pensize        => 1,
		     },$pack;
    $self->{bgcolor} = $self->translate_color(255,255,255);
    $self->{fgcolor} = $self->translate_color(0,0,0);
    return $self;
  }
}

=item GD::Simple->class('GD');

=item GD::Simple->class('GD::SVG');

Select whether new() should use GD or GD::SVG internally. Call
GD::Simple->class('GD::SVG') before calling new() if you wish to
generate SVG images.

If future GD subclasses are created, this method will subport them.

=cut

sub class {
  my $pack    = shift;
  if (@_) {
    $IMAGECLASS = shift;
    eval "require $IMAGECLASS; 1" or die $@;
    $IMAGECLASS = "$IMAGECLASS\:\:Image" 
      if $IMAGECLASS eq 'GD::SVG';
  }
  $IMAGECLASS;
}

=item $img->moveTo($x,$y)

This call changes the position of the pen without drawing. It moves
the pen to position ($x,$y) on the drawing canvas.

=cut

sub moveTo {
  my $self = shift;
  croak 'Usage GD::Simple->moveTo($x,$y)' unless @_ == 2;
  my ($x,$y) = @_;
  $self->{xy} = [$x,$y];
}

=item $img->move($dx,$dy)

=item $img->move($dr)

This call changes the position of the pen without drawing. When called
with two arguments it moves the pen $dx pixels to the right and $dy
pixels downward.  When called with one argument it moves the pen $dr
pixels along the vector described by the current pen angle.

=cut

sub move {
  my $self = shift;
  if (@_ == 1) { # polar coordinates -- this is r
    $self->{angle} += $self->{turningangle};
    my $angle = deg2rad($self->{angle});
    $self->{xy}[0] += $_[0] * cos($angle);
    $self->{xy}[1] += $_[0] * sin($angle);
  }
  elsif (@_ == 2) { # cartesian coordinates
    $self->{xy}[0] += $_[0];
    $self->{xy}[1] += $_[1];
  } else {
    croak 'Usage GD::Simple->move($dx,$dy) or move($r)';
  }
}

=item $img->lineTo($x,$y)

The lineTo() call simultaneously draws and moves the pen.  It draws a
line from the current pen position to the position defined by ($x,$y)
using the current pen size and color.  After drawing, the position of
the pen is updated to the new position.

=cut

sub lineTo {
  my $self = shift;
  croak 'Usage GD::Simple->lineTo($x,$y)' unless @_ == 2;
  $self->gd->line($self->curPos,@_,$self->fgcolor);
  $self->moveTo(@_);
}

=item $img->line($x1,$y1,$x2,$y2 [,$color])

=item $img->line($dx,$dy)

=item $img->line($dr)

The line() call simultaneously draws and moves the pen. When called
with two arguments it draws a line from the current position of the
pen to the position $dx pixels to the right and $dy pixels down.  When
called with one argument, it draws a line $dr pixels long along the
angle defined by the current pen angle.

When called with four or five arguments, line() behaves like
GD::Image->line().

=cut

sub line {
  my $self = shift;

  if (@_ >= 4) {
      my ($x1,$x2,$y1,$y2,$color) = @_;
      $color ||= $self->fgcolor;
      return $self->gd->line($x1,$x2,$y1,$y2,$color);
  }

  croak 'Usage GD::Simple->line($dx,$dy) or line($r) or line($x1,$y1,$x2,$y2 [,$color])' unless @_ >= 1;
  my @curPos = $self->curPos;
  $self->move(@_);
  my @newPos = $self->curPos;
  return $self->gd->line(@curPos,@newPos,$self->fgcolor);
}

=item $img->clear

This method clears the canvas by painting over it with the current
background color.

=cut

sub clear {
  my $self = shift;
  $self->gd->filledRectangle(0,0,$self->getBounds,$self->bgcolor);
}

=item $img->rectangle($x1,$y1,$x2,$y2)

This method draws the rectangle defined by corners ($x1,$y1),
($x2,$y2). The rectangle's edges are drawn in the foreground color and
its contents are filled with the background color. To draw a solid
rectangle set bgcolor equal to fgcolor. To draw an unfilled rectangle
(transparent inside), set bgcolor to undef.

=cut

sub rectangle {
  my $self = shift;

  return $self->gd->rectangle(@_) if @_ == 5;

  croak 'Usage GD::Simple->rectangle($x1,$y1,$x2,$y2)' unless @_ == 4;
  my $gd = $self->gd;
  my ($bg,$fg) = ($self->bgcolor,$self->fgcolor);
  $gd->filledRectangle(@_,$bg) if defined $bg;
  $gd->rectangle(@_,$fg)       if defined $fg && (!defined $bg || $bg != $fg);
}

=item $img->ellipse($width,$height)

This method draws the ellipse centered at the current location with
width $width and height $height.  The ellipse's border is drawn in the
foreground color and its contents are filled with the background
color. To draw a solid ellipse set bgcolor equal to fgcolor. To draw
an unfilled ellipse (transparent inside), set bgcolor to undef.

=cut

sub ellipse {
  my $self = shift;
  return $self->gd->ellipse(@_) if @_ == 5;

  croak 'Usage GD::Simple->ellipse($width,$height)' unless @_ == 2;
  my $gd = $self->gd;
  my ($bg,$fg) = ($self->bgcolor,$self->fgcolor);
  $gd->filledEllipse($self->curPos,@_,$bg) if defined $bg;
  $gd->ellipse($self->curPos,@_,$fg)       if defined $fg && (!defined $bg || $bg != $fg);
}

=item $img->arc([$cx,$cy,] $width,$height,$start,$end [,$style])

This method draws filled and unfilled arcs, at the current position,
with the current fore- and background colors.  See L<GD> for a
description of the arguments. To draw a solid arc (such as a pie
wedge) set bgcolor equal to fgcolor. To draw an unfilled arc, set
bgcolor to undef.

=cut

sub arc {
  my $self = shift;
  return $self->gd->arc(@_) if @_ == 7;

  croak 'Usage GD::Simple->arc($width,$height,$start,$end[,$style])' unless @_ >= 4;
  my ($width,$height,$start,$end,$style) = @_;
  my $gd = $self->gd;
  my ($bg,$fg) = ($self->bgcolor,$self->fgcolor);
  my ($cx,$cy) = $self->curPos;

  if ($bg) {
    my @args = ($cx,$cy,$width,$height,$start,$end,$bg);
    push @args,$style if defined $style;
    $gd->filledArc(@args);
  } else {
    my @args = ($cx,$cy,$width,$height,$start,$end,$fg);
    $gd->arc(@args);
  }
}

=item $img->polygon($poly)

This method draws filled and unfilled polygon using the current
settings of fgcolor for the polygon border and bgcolor for the polygon
fill color.  See L<GD> for a description of creating polygons. To draw
a solid polygon set bgcolor equal to fgcolor. To draw an unfilled
polygon, set bgcolor to undef.

=cut

sub polygon {
  my $self = shift;
  croak 'Usage GD::Simple->polygon($poly)' unless @_ == 1;
  my $gd = $self->gd;
  my ($bg,$fg) = ($self->bgcolor,$self->fgcolor);
  $gd->filledPolygon(@_,$bg) if defined $bg;
  $gd->openPolygon(@_,$fg)   if defined $fg && (!defined $bg || $bg != $fg);
}

=item $img->polyline($poly)

This method draws polygons without closing the first and last vertices
(similar to GD::Image->unclosedPolygon()). It uses the fgcolor to draw
the line.

=cut

sub polyline {
  my $self = shift;
  croak 'Usage GD::Simple->polyline($poly)' unless @_ == 1;
  my $gd = $self->gd;
  my $fg = $self->fgcolor;
  $gd->unclosedPolygon(@_,$fg);
}

=item $img->string($string)

This method draws the indicated string starting at the current
position of the pen. The pen is moved to the end of the drawn string.
Depending on the font selected with the font() method, this will use
either a bitmapped GD font or a TrueType font.  The angle of the pen
will be consulted when drawing the text. For TrueType fonts, any angle
is accepted.  For GD bitmapped fonts, the angle can be either 0 (draw
horizontal) or -90 (draw upwards).

For consistency between the TrueType and GD font behavior, the string
is always drawn so that the current position of the pen corresponds to
the bottom left of the first character of the text.  This is different
from the GD behavior, in which the first character of bitmapped fonts
hangs down from the pen point.

This method returns a polygon indicating the bounding box of the
rendered text.  If an error occurred (such as invalid font
specification) it returns undef and an error message in $@.

=cut

sub string {
  my $self   = shift;
  return $self->gd->string(@_) if @_ == 5;

  my $string = shift;
  my $font   = $self->font;
  my @bounds;
  if (ref $font && $font->isa('GD::Font')) {
    my ($x,$y) = $self->curPos;
    if ($self->angle == -90) {
      $x -= $font->height;
      $y -= $font->width;
      $self->gd->stringUp($font,$x,$y,$string,$self->fgcolor);
      $self->{xy}[1] -= length($string) * $font->width;
      @bounds = ( ($self->{xy}[0],$y), ($x,$y), ($x,$self->{xy}[1]-$font->width), ($self->{xy}[0],$self->{xy}[1]-$font->width) );
    } else {
      $y -= $font->height;
      $self->gd->string($font,$x,$y,$string,$self->fgcolor);
      $self->{xy}[0] += length($string) * $font->width;
      @bounds = ( ($x,$self->{xy}[1]), ($self->{xy}[0],$self->{xy}[1]), ($self->{xy}[0],$y), ($x,$y) );
    }
  }
  else {
    $self->useFontConfig(1);
    @bounds   = $self->stringFT($self->fgcolor,$font,
				$self->fontsize,-deg2rad($self->angle), # -pi * $self->angle/180,
				$self->curPos,$string);
    return unless @bounds;
    my ($delta_x,$delta_y) = $self->_string_width(@bounds);
    $self->{xy}[0] += $delta_x;
    $self->{xy}[1] += $delta_y;
  }
  my $poly = GD::Polygon->new;
  while (@bounds) {
    $poly->addPt(splice(@bounds,0,2));
  }
  return $poly;
}

=item $metrics = $img->fontMetrics

=item ($metrics,$width,$height) = GD::Simple->fontMetrics($font,$fontsize,$string)

This method returns information about the current font, most commonly
a TrueType font. It can be invoked as an instance method (on a
previously-created GD::Simple object) or as a class method (on the
'GD::Simple' class).

When called as an instance method, fontMetrics() takes no arguments
and returns a single hash reference containing the metrics that
describe the currently selected font and size. The hash reference
contains the following information:

  xheight      the base height of the font from the bottom to the top of
               a lowercase 'm'

  ascent       the length of the upper stem of the lowercase 'd'

  descent      the length of the lower step of the lowercase 'j'

  lineheight   the distance from the bottom of the 'j' to the top of
               the 'd'

  leading      the distance between two adjacent lines

=cut

# return %$fontmetrics
# keys: 'ascent', 'descent', 'lineheight', 'xheight', 'leading'
sub fontMetrics {
  my $self   = shift;

  unless (ref $self) {  #class invocation -- create a scratch
    $self = $self->new;
    $self->font(shift)     if defined $_[0];
    $self->fontsize(shift) if defined $_[0];
  }

  my $font   = $self->font;
  my $metrics;

  if (ref $font && $font->isa('GD::Font')) {
    my $height = $font->height;
    $metrics = {ascent     => 0,
		descent    => 0,
		lineheight => $height,
		xheight    => $height,
		leading    => 0};
  }
  else {
    $self->useFontConfig(1);
    my @mbounds   = GD::Image->stringFT($self->fgcolor,$font,
					$self->fontsize,0,
					0,0,'m');
    my $xheight   = $mbounds[3]-$mbounds[5];
    my @jbounds   = GD::Image->stringFT($self->fgcolor,$font,
					$self->fontsize,0,
					0,0,'j');
    my $ascent    = $mbounds[7]-$jbounds[7];
    my $descent   = $jbounds[3]-$mbounds[3];

    my @mmbounds  = GD::Image->stringFT($self->fgcolor,$font,
					$self->fontsize,0,
					0,0,"m\nm");
    my $twolines  = $mmbounds[3]-$mmbounds[5];
    my $lineheight  = $twolines - 2*$xheight;
    my $leading     = $lineheight - $ascent - $descent;
    $metrics     = {ascent     => $ascent,
		    descent    => $descent,
		    lineheight => $lineheight,
		    xheight    => $xheight,
		    leading    => $leading};
  }

  if ((my $string = shift) && wantarray) {
    my ($width,$height) = $self->stringBounds($string);
    return ($metrics,abs($width),abs($height));
  }
  return $metrics;
}

=item ($delta_x,$delta_y)= $img->stringBounds($string)

This method indicates the X and Y offsets (which may be negative) that
will occur when the given string is drawn using the current font,
fontsize and angle. When the string is drawn horizontally, it gives
the width and height of the string's bounding box.

=cut

sub stringBounds {
  my $self = shift;
  my $string = shift;
  my $font   = $self->font;
  if (ref $font && $font->isa('GD::Font')) {
    if ($self->angle == -90) {
      return ($font->height,-length($string) * $font->width);
    } else {
      return (length($string) * $font->width,$font->height);
    }
  }
  else {
    $self->useFontConfig(1);
    my @bounds   = GD::Image->stringFT($self->fgcolor,$font,
				       $self->fontsize,-deg2rad($self->angle),
				       $self->curPos,$string);
    return $self->_string_width(@bounds);
  }
}

=item $delta_x = $img->stringWidth($string)

This method indicates the width of the string given the current font,
fontsize and angle. It is the same as ($img->stringBounds($string))[0]

=cut

sub stringWidth {
  return ((shift->stringBounds(@_))[0]);
}


sub _string_width {
  my $self   = shift;
  my @bounds = @_;
  my $delta_x = abs($bounds[2]-$bounds[0]);
  my $delta_y = abs($bounds[5]-$bounds[3]);
  my $angle   = $self->angle % 360;
  if ($angle >= 0 && $angle < 90) {
    return ($delta_x,$delta_y);

  } elsif ($angle >= 90 && $angle < 180) {
    return (-$delta_x,$delta_y);

  } elsif ($angle >= 180 && $angle < 270) {
    return (-$delta_x,-$delta_y);

  } elsif ($angle >= 270 && $angle < 360) {
    return ($delta_x,-$delta_y);
  }
}

=item ($x,$y) = $img->curPos

Return the current position of the pen.  Set the current position
using moveTo().

=cut

sub curPos {  @{shift->{xy}}; }

=item $font = $img->font([$newfont] [,$newsize])

Get or set the current font.  Fonts can be GD::Font objects, TrueType
font file paths, or fontconfig font patterns like "Times:italic" (see
L<fontconfig>). The latter feature requires that you have the
fontconfig library installed and are using libgd version 2.0.33 or
higher.

As a shortcut, you may pass two arguments to set the font and the
fontsize simultaneously. The fontsize is only valid when drawing with
TrueType fonts.

=cut

sub font {
  my $self = shift;
  $self->{font}     = shift if @_;
  $self->{fontsize} = shift if @_;
  $self->{font};
}

=item $size = $img->fontsize([$newfontsize])

Get or set the current font size.  This is only valid for TrueType
fonts.

=cut

sub fontsize {
  my $self = shift;
  $self->{fontsize} = shift if @_;
  $self->{fontsize};
}

=item $size = $img->penSize([$newpensize])

Get or set the current pen width for use during line drawing
operations.

=cut

sub penSize {
  my $self = shift;
  if (@_) {
    $self->{pensize} = shift;
    $self->gd->setThickness($self->{pensize});
  }
  $self->{pensize};
}

=item $angle = $img->angle([$newangle])

Set the current angle for use when calling line() or move() with a
single argument. 

Here is an example of using turn() and angle() together to draw an
octagon.  The first line drawn is the downward-slanting top right
edge.  The last line drawn is the horizontal top of the octagon.

  $img->moveTo(200,50);
  $img->angle(0);
  $img->turn(360/8);
  for (1..8) { $img->line(50) }

=cut

sub angle {
  my $self = shift;
  $self->{angle} = shift if @_;
  $self->{angle};
}

=item $angle = $img->turn([$newangle])

Get or set the current angle to turn prior to drawing lines.  This
value is only used when calling line() or move() with a single
argument.  The turning angle will be applied to each call to line() or
move() just before the actual drawing occurs.

Angles are in degrees.  Positive values turn the angle clockwise.

=cut

# degrees, not radians
sub turn {
  my $self = shift;
  $self->{turningangle} = shift if @_;
  $self->{turningangle};
}

=item $color = $img->fgcolor([$newcolor])

Get or set the pen's foreground color.  The current pen color can be
set by (1) using an (r,g,b) triple; (2) using a previously-allocated
color from the GD palette; or (3) by using a symbolic color name such
as "chartreuse."  The list of color names can be obtained using
color_names(). The special color name 'transparent' will create a
completely transparent color.

=cut

sub fgcolor {
  my $self = shift;
  $self->{fgcolor} = $self->translate_color(@_) if @_;
  $self->{fgcolor};
}

=item $color = $img->bgcolor([$newcolor])

Get or set the pen's background color.  The current pen color can be
set by (1) using an (r,g,b) triple; (2) using a previously-allocated
color from the GD palette; or (3) by using a symbolic color name such
as "chartreuse."  The list of color names can be obtained using
color_names(). The special color name 'transparent' will create a
completely transparent color.

=cut

sub bgcolor {
  my $self = shift;
  $self->{bgcolor} = $self->translate_color(@_) if @_;
  $self->{bgcolor};
}

=item $index = $img->translate_color(@args)

Translates a color into a GD palette or TrueColor index.  You may pass
either an (r,g,b) triple or a symbolic color name. If you pass a
previously-allocated index, the method will return it unchanged.

=cut

sub translate_color {
  my $self = shift;
  return unless defined $_[0];
  my ($r,$g,$b);
  if (@_ == 1 && $_[0] =~ /^-?\d+/) {  # previously allocated index
    return $_[0];
  }
  elsif (@_ == 3) {  # (rgb triplet)
    ($r,$g,$b) = @_;
  }
  elsif (lc $_[0] eq 'transparent') {
      return $TRANSPARENT ||= $self->alphaColor('white',127);
  } 
  else {
    die "unknown color $_[0]" unless exists $COLORS{lc $_[0]};
    ($r,$g,$b) = @{$COLORS{lc $_[0]}};
  }
  return $self->colorResolve($r,$g,$b);
}

sub transparent {
  my $self = shift;
  my $index = $self->translate_color(@_);
  $self->gd->transparent($index);
}

=item $index = $img->alphaColor(@args,$alpha)

Creates an alpha color.  You may pass either an (r,g,b) triple or a
symbolic color name, followed by an integer indicating its
opacity. The opacity value ranges from 0 (fully opaque) to 127 (fully
transparent).

=cut

sub alphaColor {
  my $self = shift;
  return unless defined $_[0];
  my ($r,$g,$b,$a);
  if (@_ == 4) {  # (rgb triplet)
    ($r,$g,$b,$a) = @_;
  } else {
    die "unknown color $_[0]" unless exists $COLORS{lc $_[0]};
    ($r,$g,$b) = @{$COLORS{lc $_[0]}};
    $a = $_[1];
  }
  return $self->colorAllocateAlpha($r,$g,$b,$a);
}

=item @names = GD::Simple->color_names

=item $translate_table = GD::Simple->color_names

Called in a list context, color_names() returns the list of symbolic
color names recognized by this module.  Called in a scalar context,
the method returns a hash reference in which the keys are the color
names and the values are array references containing [r,g,b] triples.

=cut

sub color_names {
  my $self = shift;
  return wantarray ? sort keys %COLORS : \%COLORS;
}

=item $gd = $img->gd

Return the internal GD::Image object.  Usually you will not need to
call this since all GD methods are automatically referred to this object.

=cut

sub gd { shift->{gd} }

sub setBrush {
  my $self  = shift;
  my $brush = shift;
  if ($brush->isa('GD::Simple')) {
    $self->gd->setBrush($brush->gd);
  } else {
    $self->gd->setBrush($brush);
  }
}

=item ($red,$green,$blue) = GD::Simple->HSVtoRGB($hue,$saturation,$value)

Convert a Hue/Saturation/Value (HSV) color into an RGB triple. The
hue, saturation and value are integers from 0 to 255.

=cut

sub HSVtoRGB {
  my $self = shift;
  @_ == 3 or croak "Usage: GD::Simple->HSVtoRGB(\$hue,\$saturation,\$value)";

  my ($h,$s,$v)=@_;
  my ($r,$g,$b,$i,$f,$p,$q,$t);

  if( $s == 0 ) {
    ## achromatic (grey)
    return ($v,$v,$v);
  }
  $h %= 255;
  $s /= 255;                      ## scale saturation from 0.0-1.0
  $h /= 255;                      ## scale hue from 0 to 1.0
  $h *= 360;                      ## and now scale it to 0 to 360

  $h /= 60;                       ## sector 0 to 5
  $i = $h % 6;
  $f = $h - $i;                   ## factorial part of h
  $p = $v * ( 1 - $s );
  $q = $v * ( 1 - $s * $f );
  $t = $v * ( 1 - $s * ( 1 - $f ) );

  if($i<1) {
    $r = $v;
    $g = $t;
    $b = $p;
  } elsif($i<2){
    $r = $q;
    $g = $v;
    $b = $p;
  } elsif($i<3){
    $r = $p;
    $g = $v;
    $b = $t;
  } elsif($i<4){
    $r = $p;
    $g = $q;
    $b = $v;
  } elsif($i<5){
    $r = $t;
    $g = $p;
    $b = $v;
  } else {
    $r = $v;
    $g = $p;
    $b = $q;
  }
  return (int($r+0.5),int($g+0.5),int($b+0.5));
}

=item ($hue,$saturation,$value) = GD::Simple->RGBtoHSV($red,$green,$blue)

Convert a Red/Green/Blue (RGB) value into a Hue/Saturation/Value (HSV)
triple. The hue, saturation and value are integers from 0 to 255.

=back

=cut

sub RGBtoHSV {
  my $self = shift;
  my ($r, $g ,$bl) = @_;
  my ($min,undef,$max) = sort {$a<=>$b} ($r,$g,$bl);
  return (0,0,0) unless $max > 0;

  my $v = $max;
  my $s = 255 * ($max - $min)/$max;
  my $h;
  my $range = $max - $min;

  if ($range == 0) { # all colors are equal, so monochrome
    return (0,0,$max);
  }

  if ($max == $r) {
    $h = 60 * ($g-$bl)/$range;
  }
  elsif ($max == $g) {
    $h = 60 * ($bl-$r)/$range + 120;
  }
  else {
    $h = 60 * ($r-$g)/$range + 240;
  }

  $h += 360 if $h < 0;
  $h = int($h*255/360 + 0.5);

  return ($h, $s, $v);
}

sub newGroup {
    my $self  = shift;
    return $self->GD::newGroup(@_);
}

1;

=head1 COLORS

This script will create an image showing all the symbolic colors.

 #!/usr/bin/perl

 use strict;
 use GD::Simple;

 my @color_names = GD::Simple->color_names;
 my $cols = int(sqrt(@color_names));
 my $rows = int(@color_names/$cols)+1;

 my $cell_width    = 100;
 my $cell_height   = 50;
 my $legend_height = 16;
 my $width       = $cols * $cell_width;
 my $height      = $rows * $cell_height;

 my $img = GD::Simple->new($width,$height);
 $img->font(gdSmallFont);

 for (my $c=0; $c<$cols; $c++) {
   for (my $r=0; $r<$rows; $r++) {
     my $color = $color_names[$c*$rows + $r] or next;
     my @topleft  = ($c*$cell_width,$r*$cell_height);
     my @botright = ($topleft[0]+$cell_width,$topleft[1]+$cell_height-$legend_height);
     $img->bgcolor($color);
     $img->fgcolor($color);
     $img->rectangle(@topleft,@botright);
     $img->moveTo($topleft[0]+2,$botright[1]+$legend_height-2);
     $img->fgcolor('black');
     $img->string($color);
   }
 }

 print $img->png;

=head1 AUTHOR

The GD::Simple module is copyright 2004, Lincoln D. Stein.  It is
distributed under the same terms as Perl itself.  See the "Artistic
License" in the Perl source code distribution for licensing terms.

The latest versions of GD.pm are available at https://github.com/lstein/Perl-GD

=head1 SEE ALSO

L<GD>,
L<GD::Polyline>,
L<GD::SVG>,
L<Image::Magick>

=cut
