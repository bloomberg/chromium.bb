package Imager::Fill;
use strict;
use vars qw($VERSION);

$VERSION = "1.012";

# this needs to be kept in sync with the array of hatches in fills.c
my @hatch_types =
  qw/check1x1 check2x2 check4x4 vline1 vline2 vline4
     hline1 hline2 hline4 slash1 slosh1 slash2 slosh2
     grid1 grid2 grid4 dots1 dots4 dots16 stipple weave cross1 cross2
     vlozenge hlozenge scalesdown scalesup scalesleft scalesright stipple2
     tile_L stipple3/;
my %hatch_types;
@hatch_types{@hatch_types} = 0..$#hatch_types;

*_color = \&Imager::_color;

sub new {
  my ($class, %hsh) = @_;

  my $self = bless { }, $class;
  $hsh{combine} = Imager->_combine($hsh{combine}, 0);
  if ($hsh{solid}) {
    my $solid = _color($hsh{solid});
    if (UNIVERSAL::isa($solid, 'Imager::Color')) {
      $self->{fill} = 
        Imager::i_new_fill_solid($solid, $hsh{combine});
    }
    elsif (UNIVERSAL::isa($solid, 'Imager::Color::Float')) {
      $self->{fill} = 
        Imager::i_new_fill_solidf($solid, $hsh{combine});
    }
    else {
      $Imager::ERRSTR = "solid isn't a color";
      return undef;
    }
  }
  elsif (defined $hsh{hatch}) {
    $hsh{dx} ||= 0;
    $hsh{dy} ||= 0;
    $hsh{fg} ||= Imager::Color->new(0, 0, 0);
    if (ref $hsh{hatch}) {
      $hsh{cust_hatch} = pack("C8", @{$hsh{hatch}});
      $hsh{hatch} = 0;
    }
    elsif ($hsh{hatch} =~ /\D/) {
      unless (exists($hatch_types{$hsh{hatch}})) {
        $Imager::ERRSTR = "Unknown hatch type $hsh{hatch}";
        return undef;
      }
      $hsh{hatch} = $hatch_types{$hsh{hatch}};
    }
    my $fg = _color($hsh{fg});
    if (UNIVERSAL::isa($fg, 'Imager::Color')) {
      my $bg = _color($hsh{bg} || Imager::Color->new(255, 255, 255));
      $self->{fill} = 
        Imager::i_new_fill_hatch($fg, $bg, $hsh{combine}, 
                                 $hsh{hatch}, $hsh{cust_hatch}, 
                                 $hsh{dx}, $hsh{dy});
    }
    elsif (UNIVERSAL::isa($fg, 'Imager::Color::Float')) {
      my $bg  = _color($hsh{bg} || Imager::Color::Float->new(1, 1, 1));
      $self->{fill} = 
        Imager::i_new_fill_hatchf($fg, $bg, $hsh{combine},
                                  $hsh{hatch}, $hsh{cust_hatch}, 
                                  $hsh{dx}, $hsh{dy});
    }
    else {
      $Imager::ERRSTR = "fg isn't a color";
      return undef;
    }
  }
  elsif (defined $hsh{fountain}) {
    # make sure we track the filter's defaults
    my $fount = $Imager::filters{fountain};
    my $def = $fount->{defaults};
    my $names = $fount->{names};
    
    $hsh{ftype} = $hsh{fountain};
    # process names of values
    for my $name (keys %$names) {
      if (defined $hsh{$name} && exists $names->{$name}{$hsh{$name}}) {
        $hsh{$name} = $names->{$name}{$hsh{$name}};
      }
    }
    # process defaults
    %hsh = (%$def, %hsh);
    my @parms = @{$fount->{callseq}};
    shift @parms;
    for my $name (@parms) {
      unless (defined $hsh{$name}) {
        $Imager::ERRSTR = 
          "required parameter '$name' not set for fountain fill";
        return undef;
      }
    }

    # check that the segments supplied is an array ref
    unless (ref $hsh{segments} && $hsh{segments} =~ /ARRAY/) {
      $Imager::ERRSTR =
        "segments must be an array reference or Imager::Fountain object";
      return;
    }

    # make sure the segments are specified with colors
    my @segments;
    for my $segment (@{$hsh{segments}}) {
      my @new_segment = @$segment;

      $_ = _color($_) or return for @new_segment[3,4];
      push @segments, \@new_segment;
    }

    $self->{fill} =
      Imager::i_new_fill_fount($hsh{xa}, $hsh{ya}, $hsh{xb}, $hsh{yb},
                  $hsh{ftype}, $hsh{repeat}, $hsh{combine}, $hsh{super_sample},
                  $hsh{ssample_param}, \@segments);
  }
  elsif (defined $hsh{image}) {
    $hsh{xoff} ||= 0;
    $hsh{yoff} ||= 0;
    $self->{fill} =
      Imager::i_new_fill_image($hsh{image}{IMG}, $hsh{matrix}, $hsh{xoff}, 
                               $hsh{yoff}, $hsh{combine});
    $self->{DEPS} = [ $hsh{image}{IMG} ];
  }
  elsif (defined $hsh{type} && $hsh{type} eq "opacity") {
    my $other_fill = delete $hsh{other};
    unless (defined $other_fill) {
      Imager->_set_error("'other' parameter required to create opacity fill");
      return;
    }
    unless (ref $other_fill &&
	    eval { $other_fill->isa("Imager::Fill") }) {
      # try to auto convert to a fill object
      if (ref $other_fill && $other_fill =~ /HASH/) {
	$other_fill = Imager::Fill->new(%$other_fill)
	  or return;
      }
      else {
	undef $other_fill;
      }
      unless ($other_fill) {
	Imager->_set_error("'other' parameter must be an Imager::Fill object to create an opacity fill");
	return;
      }
    }

    my $raw_fill = $other_fill->{fill};
    my $opacity = delete $hsh{opacity};
    defined $opacity or $opacity = 0.5; # some sort of default
    $self->{fill} = 
      Imager::i_new_fill_opacity($raw_fill, $opacity);
    $self->{DEPS} = [ $other_fill ]; # keep reference to old fill and its deps
  }
  else {
    $Imager::ERRSTR = "No fill type specified";
    warn "No fill type!";
    return undef;
  }

  $self;
}

sub hatches {
  return @hatch_types;
}

sub combines {
  return Imager->combines;
}

1;

=head1 NAME

  Imager::Fill - general fill types

=head1 SYNOPSIS

  use Imager;
  use Imager::Fill;

  my $fill1 = Imager::Fill->new(solid=>$color, combine=>$combine);
  my $fill2 = Imager::Fill->new(hatch=>'vline2', fg=>$color1, bg=>$color2,
                                dx=>$dx, dy=>$dy);
  my $fill3 = Imager::Fill->new(fountain=>$type, ...);
  my $fill4 = Imager::Fill->new(image=>$img, ...);
  my $fill5 = Imager::Fill->new(type => "opacity", other => $fill,
                                opacity => ...);

=head1 DESCRIPTION 

Creates fill objects for use by most filled area drawing functions.

All fills are created with the new method.

=over

=item new

  my $fill = Imager::Fill->new(...);

The parameters depend on the type of fill being created.  See below
for details.

=back

The currently available fills are:

=over

=item *

solid

=item *

hatch

=item *

fountain (similar to gradients in paint software)

=item *

image - fill with an image, possibly transformed

=item *

opacity - a lower opacity version of some other fill

=back

=head1 Common options

=over

=item combine

The way in which the fill data is combined with the underlying image.
See L<Imager::Draw/"Combine Types">.

=back

In general colors can be specified as L<Imager::Color> or
L<Imager::Color::Float> objects.  The fill object will typically store
both types and convert from one to the other.  If a fill takes 2 color
objects they should have the same type.

=head2 Solid fills

  my $fill = Imager::Fill->new(solid=>$color, combine =>$combine)

Creates a solid fill, the only required parameter is C<solid> which
should be the color to fill with.

A translucent red fill:

  my $red = Imager::Fill->new(solid => "FF000080", combine => "normal");

=head2 Hatched fills

  my $fill = Imager::Fill->new(hatch=>$type, fg=>$fgcolor, bg=>$bgcolor,
                               dx=>$dx, $dy=>$dy);

Creates a hatched fill.  You can specify the following keywords:

=over

=item *

C<hatch> - The type of hatch to perform, this can either be the
numeric index of the hatch (not recommended), the symbolic name of the
hatch, or an array of 8 integers which specify the pattern of the
hatch.

Hatches are represented as cells 8x8 arrays of bits, which limits their
complexity.

Current hatch names are:

=over

=item *

C<check1x1>, C<check2x2>, C<check4x4> - checkerboards at various sizes

=item *

C<vline1>, C<vline2>, C<vline4> - 1, 2, or 4 vertical lines per cell

=item *

C<hline1>, C<hline2>, C<hline4> - 1, 2, or 4 horizontal lines per cell

=item *

C<slash1>, C<slash2> - 1 or 2 / lines per cell.

=item *

C<slosh1>, C<slosh2> - 1 or 2 \ lines per cell

=item *

C<grid1>, C<grid2>, C<grid4> - 1, 2, or 4 vertical and horizontal
lines per cell

=item *

C<dots1>, C<dots4>, C<dots16> - 1, 4 or 16 dots per cell

=item *

C<stipple>, C<stipple2> - see the samples

=item *

C<weave> - I hope this one is obvious.

=item *

C<cross1>, C<cross2> - 2 densities of crosshatch

=item *

C<vlozenge>, C<hlozenge> - something like lozenge tiles

=item *

C<scalesdown>, C<scalesup>, C<scalesleft>, C<scalesright> - Vaguely
like fish scales in each direction.

=item *

C<tile_L> - L-shaped tiles

=back

=item *

C<fg>, C<bg> - The C<fg> color is rendered where bits are set in the
hatch, and the C<bg> where they are clear.  If you use a transparent
C<fg> or C<bg>, and set combine, you can overlay the hatch onto an
existing image.

C<fg> defaults to black, C<bg> to white.

=item *

C<dx>, C<dy> - An offset into the hatch cell.  Both default to zero.

=back

A blue and white 4-pixel check pattern:

  my $fill = Imager::Fill->new(hatch => "check2x2", fg => "blue");

You can call Imager::Fill->hatches for a list of hatch names.

=head2 Fountain fills

  my $fill = Imager::Fill->new(fountain=>$ftype, 
       xa=>$xa, ya=>$ya, xb=>$xb, yb=>$yb, 
       segments=>$segments, repeat=>$repeat, combine=>$combine, 
       super_sample=>$super_sample, ssample_param=>$ssample_param);

This fills the given region with a fountain fill.  This is exactly the
same fill as the C<fountain> filter, but is restricted to the shape
you are drawing, and the fountain parameter supplies the fill type,
and is required.

A radial fill from white to transparent centered on (50, 50) with a 50
pixel radius:

  use Imager::Fountain;
  my $segs = Imager::Fountain->simple(colors => [ "FFFFFF", "FFFFFF00" ],
                                      positions => [ 0, 1 ]);
  my $fill = Imager::Fill->new(fountain => "radial", segments => $segs,
                               xa => 50, ya => 50, xb => 0, yb => 50,
                               combine => "normal");


=head2 Image Fills

  my $fill = Imager::Fill->new(image=>$src, xoff=>$xoff, yoff=>$yoff,
                               matrix=>$matrix, combine => $combine);

Fills the given image with a tiled version of the given image.  The
first non-zero value of C<xoff> or C<yoff> will provide an offset
along the given axis between rows or columns of tiles respectively.

The matrix parameter performs a co-ordinate transformation from the
co-ordinates in the target image to the fill image co-ordinates.
Linear interpolation is used to determine the fill pixel.  You can use
the L<Imager::Matrix2d> class to create transformation matrices.

The matrix parameter will significantly slow down the fill.

  # some image to act as a texture
  my $txim = Imager->new(...);

  # simple tiling
  my $fill = Imager::Fill->new(image => $txim);

  # tile with a vertical offset
  my $fill = Imager::Fill->new(image => $txim, yoff => 10);

  # tile with a horizontal offset
  my $fill = Imager::Fill->new(image => $txim, xoff => 10);

  # rotated
  use Imager::Matrix2d;
  my $fill = Imager::Fill->new(image => $txim,
                matrix => Imager::Matrix2d->rotate(degrees => 20));

=head2 Opacity modification fill

  my $fill = Imager::Fill->new(type => "opacity",
      other => $fill, opacity => 0.25);

This can be used to make a fill that is a more translucent or opaque
version of an existing fill.  This is intended for use where you
receive a fill object as a parameter and need to change the opacity.

Parameters:

=over

=item *

type => "opacity" - Required

=item *

other - the fill to produce a modified version of.  This must be an
Imager::Fill object.  Required.

=item *

opacity - multiplier for the source fill opacity.  Default: 0.5.

=back

The source fills combine mode is used.

  my $hatch = Imager::Fill->new(hatch => "check4x4", combine => "normal");
  my $fill = Imager::Fill->new(type => "opacity", other => $hatch);

=head1 OTHER METHODS

=over

=item Imager::Fill->hatches

A list of all defined hatch names.

=item Imager::Fill->combines

A list of all combine types.

=back

=head1 FUTURE PLANS

I'm planning on adding the following types of fills:

=over

=item *

C<checkerboard> - combines 2 other fills in a checkerboard

=item *

C<combine> - combines 2 other fills using the levels of an image

=item *

C<regmach> - uses the transform2() register machine to create fills

=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
