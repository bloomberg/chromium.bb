#!perl

#
# Simple convertor from bdf to loadable GD font format.
#
# Author: Lincoln Stein <lstein@cshl.edu>, heavily adopted from bdftogd from
# Jan Pazdziora <adelton@fi.muni.cz>
#
# Example of use:
# fstobdf -s fontserverhost:7100 -fn 8x16 | bdftofnt > myfont.fnt
#

use strict;
our $VERSION = '1.00';

if ($ARGV[0] =~ /^--?h/) {
  exec "perldoc $0";
}

my ($width, $height);
my (@data, @left, @bottom);
my ($globalleft, $globaltop);

my ($minchar, $maxchar);

my ($copyright, $fontdef);

my $currentchar;
my $gobitmap = 0;

foreach (@ARGV) {
  $_ = "gunzip -c $_ |" if /\.gz$/;
}

while (<>)
  {
    chomp;
    s/\r$//;
    next unless $_;
    my ($tag, $value) = split / /, $_, 2;
    die "Font is not fixed width\n"
      if $tag eq 'SPACING' and not $value =~ /[CM]/i;

    $currentchar = $value if $tag eq 'ENCODING';
    $minchar = $currentchar if not defined $minchar
      or ($currentchar < $minchar && $currentchar >= 0);
    $maxchar = $currentchar if not defined $maxchar
      or ($currentchar > $maxchar && $currentchar >= 0);
	
    if ($tag eq 'ENDCHAR')
      {
	next if $currentchar < 0;
	$gobitmap = 0;
	my $bottom = $globaltop - $bottom[$currentchar];
		

	if ($bottom > 0)
	  { $data[$currentchar] = substr $data[$currentchar], 0, length($data[$currentchar]) - $bottom * $width; }
	else
	  { $data[$currentchar] .= '0' x (-$bottom * $width); }
      }

    if ($tag eq 'FONTBOUNDINGBOX')
      {
	my ($tag, $wid, $hei, $left, $top) = split / /;
	if (defined $top)
	  {
	    $globalleft = $left;
	    $globaltop = $top;
	    $height = $hei;
	    $width = $wid;
	  }
      }
    if ($tag eq 'FONT' and not defined $fontdef)
      { $fontdef = $value; }
    if ($tag eq 'COPYRIGHT' and not defined $copyright)
      { $copyright = $value; }
	
    if ($tag eq 'BBX')
      {
	my ($tag, $wid, $hei, $left, $bottom) = split / /;
	if (defined $bottom)
	  {
	    $left[$currentchar] = $left;
	    $bottom[$currentchar] = $bottom;
	  }
      }

    if ($gobitmap)
      {
	my $value = pack 'H*', $_;
	my $bits = unpack 'B*', $value;
	$bits = ('0' x $left[$currentchar]) . $bits;
	$bits .= '0' x ($width - length $bits);
	$bits = substr $bits, 0, $width;
	$data[$currentchar] .= $bits;
      }
	
    if ($tag eq 'BITMAP')
      {
	$gobitmap = 1;
	$data[$currentchar] = '';
      }
  }

$minchar = 0   unless defined $minchar;
$maxchar = 255 unless defined $maxchar;

binmode STDOUT;  # for DOS/Windows systems
my $length = $maxchar-$minchar+1;


print pack ('VVVV',$length,$minchar,$width,$height);  # header

for (my $i = $minchar; $i <= $maxchar; $i++) {
  $data[$i] = '' unless defined $data[$i];
  $data[$i] = '0' x ($width * $height - length $data[$i]) . $data[$i];
  print pack('C*',split '',$data[$i]);
}

print STDERR "Successfully converted $length ${width}x$}height} characters\n";

__END__

=head1 NAME

bdf2gdfont.pl - Convert X11 "BDF" fonts into a loadable font format for GD.

=head1 SYNOPSIS

  % bdf2gdfont.pl courR12.bdf > courR12.fnt

=head1 DESCRIPTION

This script converts BDF-style X11 font files into a format that can
be loaded by the GD module using the GD::Font->load() method.  There
are a number of ways to obtain BDF fonts.

=over 4

=item 1. The font is already present on your system.

Some BDF fonts can be found in the standard X11R6 distribution. This
script will automatically uncompress gzipped font files if their
extension ends with .gz (the gunzip program must be on your path).

=item 2. From a font server.

The "fstobdf" utility, a standard X11 utility, will read a named font
from the font server of your choice and return it in BDF format.  You
can pipe it to bdf2gdfont.pl:

  fstobdf -s fontserverhost:7100 -fn 8x16 | bdf2gdfont.pl > newfont.fnt

Use xlsfonts to find out what fonts are available.  Most fonts will
have long names like
-B&H-LucidaTypewriter-Bold-R-Normal-Sans-18-180-75-75-M-110-ISO8859-10.

=item 3. Using the pcf2bdf utility.

Some fonts are only available in PCF (compiled) format.  To obtain
these, you can either turn on a font server and follow recipe (2), or
use TAGA Nayuta's pcf2bdf utility. This utility is available from
http://www.tsg.ne.jp/GANA/S/pcf2bdf/ (page is in Japanese, but you can
find the download link).

=back

=head2 Limitations

This font converter only works with fixed-width fonts.  If used with a
TrueType or proportional font it will die with an error message.

=head1 SEE ALSO

L<GD>

=head1 AUTHOR

Lincoln Stein <lstein@cshl.org>, heavily adapted from bdftogd from
Jan Pazdziora <adelton@fi.muni.cz>.

Copyright (c) 2004 Cold Spring Harbor Laboratory

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

