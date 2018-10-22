package Imager::Font::Image;
use strict;
use Imager::Color;
use File::Basename;
use File::Spec;

use vars qw(@ISA %REQUIRED_FIELDS);
@ISA = qw(Imager::Font);

sub NWIDTH   () { 0 }
sub PWIDTH   () { 2 }
sub GDESCENT () { 1 }
sub GASCENT  () { 3 }
sub DESCENT  () { 4 }
sub ASCENT   () { 5 }


%REQUIRED_FIELDS = (
	Image_spec     => 1,
	Font_size      => 1,
	Global_ascent  => 1,
	Global_descent => 1,);

# Required fields
# Fontmetrics:
# Font global data:
#   image name
#   font size
#   max glyph height
#   max glyph width
#
# The per character data is:
#   left edge   (inclusive)
#   right edge  (exclusive)
#   top edge    (inclusive)
#   bottom edge (exclusive)
#   left adjustment
#   forward shift
#   baseline adjustment (from top)
#
# The left adjustment is the starting
# offset into the glyph, the forward shift
# is the actual forward movement of the
# imaginary cursor.

# To calculate the size of a string use:
#  sum (forward_shift_i) + left_adjustment_0 + width_last - left_adjustment_last - forward_shift_last

# example font spec file:

# IAGRFONT
# # This is an imager font definition file.  This is a comment
# Image_spec = foo.png
# Font_size  = 12
# Global_ascent = 10
# Global_descent = -2
# # Per character data
# FM_65 = 20 40 30 50 3 15
# # Code for 'A' left edge = 20, right = 40, top = 30, bottom 50, leading = 3, forward = 15.
# The left adjustment is the starting
# offset into the glyph, the forward shift
# is the actual forward movement of the
# imaginary cursor.

# To calculate the size of a string use:
#  sum (forward_shift_i) + left_adjustment_0 + width_last - left_adjustment_last - forward_shift_last



sub parse_fontspec_file {
  my ($self, $file) = @_;
  local *FH;
  return unless open(FH, "<$file");

  my %req = %REQUIRED_FIELDS;

  while(<FH>) {
    next if m/^\#/;
    if (m/^\s*?(\S+?)\s*=\s*(.+?)\s*$/) {
      # Check for a required field:
      my $char = $1;
      my $metric = $2;
      if ($req{$char}) {
	$self->{$char} = $metric;
	delete $req{$1};
      } else {
	next unless $char =~ s/^FM_(\d+)$/$1/;
	next unless $metric =~ m/(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)$/;
	$self->{fm}->{$char} = [$1, $2, $3, $4, $5, $6];
      }
    }
  }
  close(FH);
  return $self;
}



sub new {
  my $self = bless {}, shift;
  my %opts = (color=>Imager::Color->new(255, 0, 0, 0), @_);

  unless ($opts{file}) {
    $Imager::ERRSTR = "No font file specified";
    return;
  }
  unless ($self->parse_fontspec_file($opts{file})) {
    $Imager::ERRSTR = "Font file $opts{file} not found or bad";
    return;
  }

  my $img = Imager->new();
  my $img_filename = File::Spec->catfile( dirname($opts{'file'}),
					  $self->{Image_spec} );

  unless ($img->open(%opts, file=>$img_filename)) {
    $Imager::ERRSTR = "Font IMAGE file $img_filename not found or bad: ".
      $img->errstr();
    return;
  }

  $self->{image} = $img;
  $self->{size} = $self->{Font_size};
  return $self;
}

sub get_glyph_data {
  my ($self, $glyph_code) = @_;
  return unless exists $self->{fm}->{$glyph_code};
  return @{$self->{fm}->{$glyph_code}};
}

# copy_glyph
#
# $x, $y is left, baseline for glyphs.
#

sub copy_glyph {
  my ($self, $glyph_code, $target_img, $x, $y) = @_;

  my @gdata = $self->get_glyph_data($glyph_code) or return;

  $target_img->rubthrough(src=>$self->{image},
			  tx => $x + $gdata[4],
			  ty => $y -  $self->{Global_ascent},,
			  src_minx => $gdata[0],
			  src_maxx => $gdata[1],
			  src_miny => $gdata[2],
			  src_maxy => $gdata[3]);
}

sub _draw {
  my ($self, %opts) = @_;

  my $x = $opts{'x'};
  my $y = $opts{'y'};

  my @glyphs = unpack("C*", $opts{string});
  my $img = $opts{image};

  my $glyph;
  for $glyph (@glyphs) {
    my @gmetrics = $self->get_glyph_data($glyph) or next;
    $self->copy_glyph($glyph, $img, $x, $y);
    $x += $gmetrics[5];
  }
}
