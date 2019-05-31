package Imager::Transform;
use strict;
use Imager;
use Imager::Expr::Assem;
use vars qw($VERSION);

$VERSION = "1.006";

my %funcs =
  (
   mandel=>
   {
    desc=>"Mandelbrot set",
    type=>'assem',
    assem=><<EOS,
# x treated as in range minx..maxx
# y treated as in range miny..maxy
    var nx:n ; var ny:n
    var diffx:n ; var diffy:n
# conx/y are x/y adjusted to min..max ranges
    var conx:n ; var cony:n
    diffx = subtract maxx minx
    conx = div x w
    conx = mult conx diffx
    conx = add conx minx
    diffy = subtract maxy miny
    cony = div y h
    cony = mult cony diffy
    cony = add cony miny
    nx = 0
    ny = 0
    var count:n
    count = 0
loop:
# calculate (nx,ny)**2 +(x,y)->
#  (nx*nx-ny*ny+x, 2.nx.ny+y)
    var wx:n ; var wy:n ; var work:n
    wx = mult nx nx
    wy = mult ny ny
    wx = subtract wx wy
    ny = mult ny nx
    ny = mult ny 2
    nx = wx
    nx = add nx conx
    ny = add ny cony
    work = distance nx ny 0 0
    work = gt work 2
    jumpnz work docol
    count = add count 1
    work = lt count maxcount
    jumpnz work loop
    jumpnz insideangle doinang
    var workp:p
    workp = rgb 0 0 0
    ret workp
  doinang:
    var ang:n
    ang = atan2 ny nx
    ang = mult ang 360
    ang = div ang pi
    workp = hsv ang 255 0.5
    ret workp
  docol:
    var outvalue:n
    outvalue = mult outsidevaluestep count
    outvalue = add outvalue outsidevalue
    outvalue = mod outvalue 1.01
    jumpnz outsideangle do_outang
    work = mult count huestep
    work = add work huebase
    work = mod work 360
    workp = hsv work 1 outvalue
    ret workp
  do_outang:
    ang = atan2 ny nx
    ang = mult ang 360
    ang = div ang pi
    ang = add ang outsidebase
    workp = hsv ang outsidesat outvalue
    ret workp
EOS
    constants=>
    {
     minx=>{ default=>-2, desc=>'Left of rendered area', },
     miny=>{ default=>-1.5, desc=>'Top of rendered area', },
     maxx=>{ default=>1, desc=>'Right of rendered area', },
     maxy=>{ default=>1.5, desc=>'Bottom of rendered area', },
     maxcount=>{ default=>100, desc=>'Maximum iterations', },
     huestep=>{ default=>21.1, desc=>'Hue step for number of iterations', },
     huebase=>{ default=>0, desc=>'Base hue for number of iterations', },
     insideangle=>
     { 
      default=>0, 
      desc=>'Non-zero to use angle of final as hue for inside',
     },
     insidebase=>
     {
      default=>0,
      desc=>'Base angle for inside colours if insideangle is non-zero',
     },
     outsideangle=>
     { 
      default=>0, 
      desc=>'Non-zero to use angle of final as hue for outside',
     },
     outsidebase=>
     {
      default=>0,
      desc=>'Base angle if outsideangle is true',
     },
     outsidevalue=>
     {
      default=>1,
      desc=>'Brightness for outside pixels',
     },
     outsidevaluestep=>
     {
      default=>0,
      desc=>'Brightness step for each count for outside pixels',
     },
     outsidesat=>
     {
      default=>1,
      desc=>'Saturation for outside pixels',
     },
    },
    inputs=>[],
   },
   julia=>
   {
    desc=>"Julia set",
    type=>'assem',
    assem=><<EOS,
#    print x
# x treated as in range minx..maxx
# y treated as in range miny..maxy
    var nx:n ; var ny:n
    var diffx:n ; var diffy:n
# conx/y are x/y adjusted to min..max ranges
    var conx:n ; var cony:n
    diffx = subtract maxx minx
    conx = div x w
    conx = mult conx diffx
    conx = add conx minx
    diffy = subtract maxy miny
    cony = div y h
    cony = mult cony diffy
    cony = add cony miny
    nx = conx
    ny = cony
    var count:n
    count = 0
loop:
# calculate (nx,ny)**2 +(x,y)->
#  (nx*nx-ny*ny+x, 2.nx.ny+y)
    var wx:n ; var wy:n ; var work:n
    wx = mult nx nx
    wy = mult ny ny
    wx = subtract wx wy
    ny = mult ny nx
    ny = mult ny 2
    nx = wx
    nx = add nx zx
    ny = add ny zy
    work = distance nx ny 0 0
    work = gt work 2
    jumpnz work docol
    count = add count 1
    work = lt count maxcount
    jumpnz work loop
    jumpnz insideangle doinang
    var workp:p
    workp = rgb 0 0 0
    ret workp
  doinang:
    var ang:n
    ang = atan2 ny nx
    ang = mult ang 360
    ang = div ang pi
    workp = hsv ang 255 0.5
    ret workp
  docol:
    var outvalue:n
    outvalue = mult outsidevaluestep count
    outvalue = add outvalue outsidevalue
    outvalue = mod outvalue 1.01
    jumpnz outsideangle do_outang
    work = mult count huestep
    work = add work huebase
    work = mod work 360
    workp = hsv work 1 outvalue
    ret workp
  do_outang:
    ang = atan2 ny nx
    ang = mult ang 360
    ang = div ang pi
    ang = add ang outsidebase
    workp = hsv ang outsidesat outvalue
    ret workp
EOS
    constants=>
    {
     zx=>{default=>0.7, desc=>'Real part of initial Z', },
     zy=>{default=>0.2, desc=>'Imaginary part of initial Z', },
     minx=>{ default=>-1.5, desc=>'Left of rendered area', },
     miny=>{ default=>-1.5, desc=>'Top of rendered area', },
     maxx=>{ default=>1.5, desc=>'Right of rendered area', },
     maxy=>{ default=>1.5, desc=>'Bottom of rendered area', },
     maxcount=>{ default=>100, desc=>'Maximum iterations', },
     huestep=>{ default=>21.1, desc=>'Hue step for number of iterations', },
     huebase=>{ default=>0, desc=>'Base hue for number of iterations', },
     insideangle=>
     { 
      default=>0, 
      desc=>'Non-zero to use angle of final as hue for inside',
     },
     insidebase=>
     {
      default=>0,
      desc=>'Base angle for inside colours if insideangle is non-zero',
     },
     outsideangle=>
     { 
      default=>0, 
      desc=>'Non-zero to use angle of final as hue for outside',
     },
     outsidebase=>
     {
      default=>0,
      desc=>'Base angle if outsideangle is true',
     },
     outsidevalue=>
     {
      default=>1,
      desc=>'Brightness for outside pixels',
     },
     outsidevaluestep=>
     {
      default=>0,
      desc=>'Brightness step for each count for outside pixels',
     },
     outsidesat=>
     {
      default=>1,
      desc=>'Saturation for outside pixels',
     },
    },
    inputs=>[],
   },
   circleripple=>
   {
    type=>'rpnexpr',
    desc=>'Adds a circular ripple effect',
    rpnexpr=><<'EOS',
x y cx cy distance !dist
@dist freq / sin !scale
@scale depth * @dist + !adj
y cy - x cx - atan2 !ang
cx @ang cos @adj * + cy @ang sin @adj * + getp1 @scale shadow + shadow 1 + / *
EOS
    constants=>
    {
     freq=> { desc=>'Frequency of ripples', default=>5 },
     depth=> { desc=>'Depth of ripples', default=>10 },
     shadow=> { desc=>'Fraction of shadow', default=>20 },
    },
    inputs=>
	[
	 { desc=>'Image to ripple' }
	 ],
   },
   spiral=>
   {
    type=>'rpnexpr',
    desc=>'Render a colorful spiral',
    rpnexpr=><<'EOS',
x y cx cy distance !d y cy - x cx - atan2 !a
@d spacing / @a + pi 2 * % !a2 
@a 180 * pi / 1 @a2 sin 1 + 2 / hsv
EOS
    constants=>
    {
     spacing=>{ desc=>'Spacing between arms', default=>10 },
    },
    inputs=>[],
   },
   diagripple=>
   {
    type=>'rpnexpr',
    desc=>'Adds diagonal ripples to an image',
    rpnexpr=><<'EOS',
x y + !dist @dist freq / sin !scale 
@scale depth * !adj
 x @adj + y @adj + getp1 @scale shadow + shadow 1 + / *
EOS
    constants=> 
    {
     freq=>{ desc=>'Frequency of ripples', default=>5, },
     depth=>{desc=>'Depth of ripples', default=>3,},
     shadow=>
     {
	 desc=>'Fraction of brightness to remove for shadows',
	 default=>20,
     },
    },
    inputs=>
	[
	 { desc=>'Image to add ripples to' }
	 ],
   },
   twist=>
   {
    type=>'rpnexpr',
    desc=>'Twist an image',
    rpnexpr=><<'EOS',
x y cx cy distance !dist
 y cy - x cx - atan2 @dist twist / + !ang
cx @ang cos @dist * + cy @ang sin @dist * + getp1
EOS
    constants=>
    {
     twist=>{ desc=>'Amount of twist', default=>2.5, },
    },
    inputs=>
    [
     { desc=>'Image to twist' },
    ],
   },
   # any other functions can wait until Imager::Expr::Infix supports
   # jumps
  );

sub new {
  my ($class, $name) = @_;

  exists $funcs{$name} or return;

  bless { func=>$funcs{$name}, name=>$name }, $class;
}

sub inputs {
  my ($self) = @_;
  return @{$self->{func}{inputs}}
}

sub constants {
  my $self = shift;
  if (@_) {
    return @{$self->{func}{constants}}{@_};
  }
  else {
    return keys %{$self->{func}{constants}};
  }
}

sub transform {
  my ($self, $opts, $constants, @in) = @_;

  my $func = $self->{func};
  my %opts = %$opts;
  $opts{$func->{type}} = $func->{$func->{type}};
  my %con = %$constants;
  for my $name (keys %{$func->{'constants'}}) {
    unless (exists $con{$name}) {
      if (exists $func->{'constants'}{$name}{default}) {
	$con{$name} = $func->{'constants'}{$name}{default};
      }
      else {
	$self->{error} = "No value or default for constant $name";
	return;
      }
    }
  }
  $opts{'constants'} = \%con;
  unless (@in == @{$func->{'inputs'}}) {
    $self->{error} = @in." input images given, ".
      @{$func->{'inputs'}}." supplied";
    return;
  }

  my $out = Imager::transform2(\%opts, @in);
  unless ($out) {
    $self->{error} = $Imager::ERRSTR;
    return;
  }
  return $out;
}

sub errstr {
  return $_[0]{error};
}

sub list {
  return keys %funcs;
}

sub describe {
  my ($class, $name) = @_;

  my $func;
  if (ref $class && !$name) {
    $func = $class->{func};
    $name = $class->{name}
  }
  else {
    $func = $funcs{$name}
      or return undef;
  }
  my $desc = <<EOS;
Function   : $name
Description: $func->{desc}
EOS
  if ($func->{'inputs'} && @{$func->{'inputs'}}) {
    $desc .= "Input images:\n";
    my $i = 1;
    for my $in (@{$func->{'inputs'}}) {
      $desc .= "  $i: $in->{desc}\n";
    }
  }
  else {
    $desc .= "There are no input images\n";
  }
  if ($func->{'constants'} && keys %{$func->{'constants'}}) {
    $desc .= "Input constants:\n";
    for my $key (keys %{$func->{'constants'}}) {
      $desc .= "  $key: $func->{'constants'}{$key}{desc}\n";
      $desc .= "       Default: $func->{'constants'}{$key}{default}\n";
    }
  }
  else {
    $desc .= "There are no constants\n";
  }

  return $desc;
}


1;

__END__

=head1 NAME

  Imager::Transform - a library of register machine image transformations

=head1 SYNOPSIS

  # get a list of transformations
  my @funcs = Imager::Transform->list;
  # create a transformation object
  my $tran = Imager::Transform->new($name);
  # describe it
  print $tran->describe;
  # a list of constant names
  my @constants = $tran->constants;
  # information about some of the constants
  my @info = $tran->constants(@constants);

=head1 DESCRIPTION

This module provides a library of transformations that use the Imager
transform2() function.

The aim is to provide a place to collect these transformations.

At some point there might be an interface to add new functions, but
there's not a whole lot of point to that.

The interface is a little sparse as yet.

=head1 METHODS

=over 4

=item my @names = Imager::Transform->list

Returns a list of the transformations.

=item my $desc = Imager::Transform->describe($name);

=item my $desc = $tran->describe()

Describes a transformation specified either by name (as a class
method) or by reference (as an instance method).

The class method returns undef if there is no such transformation.

=item my $tran = Imager::Transform->new($name)

Create a new transformation object.  Returns undef if there is no such
transformation.

=item my @inputs = $tran->inputs;

=item my $inputs = $tran->inputs;

Returns a list of input image descriptions, or the number of them,
depending on content.

The list contains hash references, which current contain only one
member, C<desc>, a description of the use of the input image.

=item $tran->constants

Returns a list of names of constants that can be set for the
transformation.

=item $tran->constants($name, $name, ...)

Returns a hashref for each named constant, which contains the default
in key C<default> and a description in key C<desc>.

=item my $out = $tran->transform(\%opts, \%constants, @imgs)

Perform the image transformation.

Returns the new image on success, or undef on failure, in which case
you can use $tran->errstr to get an error message.

=item $tran->errstr

The error message, if any from the last image transformation.

=back

=head1 BUGS

Needs more transformations.

=head1 SEE ALSO

Imager(3), F<transform.perl>

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=cut
