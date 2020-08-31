package Imager::Font::Test;
use strict;

our $VERSION = "1.0001";

use base 'Imager::Font';

sub new {
  my ($class, %opts) = @_;

  bless \%opts, shift;
}

sub _draw {
  my ($self, %input) = @_;

  my $text = $input{string};

  my $ppn = int($input{size} * 0.5 + 0.5);
  my $desc = int($input{size} * 0.3 + 0.5);
  my $asc = $input{size} - $desc;
  my $width = $ppn * length $text;
  my $x = $input{x};
  my $y = $input{'y'};
  $input{align} and $y -= $asc;

  $input{image}->box(color => $input{color}, xmin => $x, ymin => $y,
		     xmax => $x + $width-1, ymax => $y + $input{size} - 1);

  return 1;
}

sub _bounding_box {
  my ($self, %input) = @_;

  my $text = $input{string};

  my $ppn = int($input{size} * 0.5 + 0.5);
  my $desc = int($input{size} * 0.3 + 0.5);
  my $asc = $input{size} - $desc;

  return ( 0, -$desc, $ppn * length $text, $asc, -$desc, $asc, $ppn * length $text, 0 );
}

sub has_chars {
  my ($self, %input) = @_;

  my $text = $input{string};
  defined $text
    or return Imager->_set_error("has_chars: No string parameter supplied");

  return (1) x length $text;
}

sub face_name {
  "test";
}

sub glyph_names {
  my ($self, %input) = @_;

  my $text = $input{string};
  defined $text
    or return Imager->_set_error("glyph_names: No string parameter supplied");

  return (1) x length $text;
}

1;

=head1 NAME

Imager::Font::Test - font driver producing consistent output for tests.

=head1 SYNOPSIS

  my $font = Imager::Font::Test->new;

  # use $font where you use other fonts

=head1 DESCRIPTION

Imager::Font::Test is intended to produce consistent output without
being subject to the inconsistent output produced by different
versions of font libraries.

The output is simple box for the whole string.

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=cut

