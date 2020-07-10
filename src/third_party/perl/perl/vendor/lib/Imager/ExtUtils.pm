package Imager::ExtUtils;
use strict;
use File::Spec;

use vars qw($VERSION);

$VERSION = "1.002";

=head1 NAME

Imager::ExtUtils - functions handy in writing Imager extensions

=head1 SYNOPSIS

  # make Imager easier to use with Inline
  # perldoc Imager::Inline
  use Inline with => 'Imager';

=head1 DESCRIPTION

=over

=item base_dir

Returns the base directory where Imager is installed.

=cut

# figure out where Imager is installed
sub base_dir {
  for my $inc_dir (@INC) {
    if (-e "$inc_dir/Imager.pm") {
      my $base_dir = $inc_dir;
      unless (File::Spec->file_name_is_absolute($base_dir)) {
	$base_dir = File::Spec->rel2abs($base_dir);
      }
      return $base_dir;
    }
  }

  die "Cannot locate an installed Imager!";
}

=item inline_config

Implements Imager's Inline::C C<with> hook.

=cut

sub inline_config {
  my ($class) = @_;
  my $base = base_dir();

  return
    {
     INC => $class->includes,
     TYPEMAPS => $class->typemap,
     AUTO_INCLUDE => <<CODE,
/* Inserted by Imager $Imager::VERSION */
#include "imext.h"
#include "imperl.h"
DEFINE_IMAGER_CALLBACKS;
CODE
     BOOT => 'PERL_INITIALIZE_IMAGER_CALLBACKS;',
     FILTERS => \&_inline_filter,
    };
}

my @inline_replace =
  qw(
   Imager::ImgRaw
   Imager::Color::Float
   Imager::Color
   Imager::IO
  );

my %inline_replace =
  map { (my $tmp = $_) =~ s/::/__/g; $_ => $tmp } @inline_replace;

my $inline_replace_re = "\\b(" . join('|', @inline_replace) . ")\\b";

sub _inline_filter {
  my $code = shift;

  $code =~ s/$inline_replace_re/$inline_replace{$1}/g;

  $code;
}

=item includes

Returns -I options suitable for use with ExtUtils::MakeMaker's INC
option.

=cut

sub includes {
  my $class = shift;
  my $base = $class->base_dir();

  "-I" . $base . '/Imager/include',
}

=item typemap

Returns the full path to Imager's installed typemap.

=cut

sub typemap {
  my $class = shift;
  my $base = $class->base_dir();

  $base . '/Imager/typemap';
}

1;

__END__

=back

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=head1 REVISION

$Revision$

=head1 SEE ALSO

Imager, Imager::API, Imager::Inline, Imager::APIRef.

=cut
