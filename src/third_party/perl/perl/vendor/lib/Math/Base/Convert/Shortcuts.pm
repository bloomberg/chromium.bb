package Math::Base::Convert::Shortcuts;

use vars qw($VERSION);
use strict;

$VERSION = do { my @r = (q$Revision: 0.05 $ =~ /\d+/g); sprintf "%d."."%02d" x $#r, @r };

# load bitmaps

my $xlt = require Math::Base::Convert::Bitmaps;

#
#	    base	     2          4     8	     16	       32	 64
#	  base power	   1	      2	    3	   4	      5	       6
# xlt = [ \@standardbases, undef, \%_2wide, undef, undef, \%_5wide, \%_6wide ]; 
#
#	base 2	maps directly to lookup key
#	base 3	maps directly to standard lookup value
#	base 4	converts directly to hex
#
# where @standardbases = (\{
#	dna	=> {
#			'00'	=> 'a',
#			'01'	=> 'c',
#			'10'	=> 't',
#			'11'	=> 'g',
#	},
#	b64	=> {
#			'000000' =>  0,
#			'000001' =>  1,
#			   *	     -
#			   *	     -
#			'001010' => 'A',
#			'001011' => 'B',
#			   *	     -
#			   *	     -
#			'111111' => '_',
#	},
#	m64	=>	etc....
#	iru
#	url
#	rex
#	id0
#	id1
#	xnt
#	xid
# });
#
# .... and
#
# hash arrays are bit to value maps of the form
#
# %_3wide = {
#	'000'	=> 0,
#	'001'	=> 1,
#	'010'	=> 2,
#	  *	   -
#	  *	   -
#	etc...
# };
#

my @srindx = (	# accomodate up to 31 bit shifts
	0,		# 0 unused
	1,		# 1
	3,		# 2
	7,		# 3
	0xf,		# 4
	0x1f,		# 5
	0x3f,		# 6
	0x7f,		# 7
	0xff,		# 8
	0x1ff,		# 9
	0x3ff,		# 10
	0x7ff,		# 11
	0xfff,		# 12
	0x1fff,		# 13
	0x3fff,		# 14
	0x7fff,		# 15
	0xffff,		# 16
	0x1ffff,	# 17
	0x3ffff,	# 18
	0x7ffff,	# 19
	0xfffff,	# 20
	0x1fffff,	# 21
	0x3fffff,	# 22
	0x7fffff,	# 23
	0xffffff,	# 24
	0x1ffffff,	# 25
	0x3ffffff,	# 26
	0x7ffffff,	# 27
	0xfffffff,	# 28
	0x1fffffff,	# 29
	0x3fffffff,	# 30
	0x7fffffff	# 31
);

my @srindx2 = (	# accomodate up to 31 bit shifts
	0xffffffff,	# 0 unused
	0xfffffffe,	# 1
	0xfffffffc,	# 2
	0xfffffff8,	# 3
	0xfffffff0,	# 4
	0xffffffe0,	# 5
	0xffffffc0,	# 6
	0xffffff80,	# 7
	0xffffff00,	# 8
	0xfffffe00,	# 9
	0xfffffc00,	# 10
	0xfffff800,	# 11
	0xfffff000,	# 12
	0xffffe000,	# 13
	0xffffc000,	# 14
	0xffff8000,	# 15
	0xffff0000,	# 16
	0xfffe0000,	# 17
	0xfffc0000,	# 18
	0xfff80000,	# 19
	0xfff00000,	# 20
	0xffe00000,	# 21
	0xffc00000,	# 22
	0xff800000,	# 23
	0xff000000,	# 24
	0xfe000000,	# 25
	0xfc000000,	# 26
	0xf8000000,	# 27
	0xf0000000,	# 28
	0xe0000000,	# 29
	0xc0000000,	# 30
	0x80000000	# 31
);

#
# $arraypointer, $shiftright, $mask, $shiftleft
#
sub longshiftright {
  my $ap  = $_[0];		# perl appears to optimize these variables into registers
  my $sr  = $_[1];		# when they are set in this manner -- much faster!!
  my $msk = $_[2];   
  my $sl  = $_[3];
  my $al  = $#$ap -1; 
  my $i   = 1;
  foreach (0..$al) {
    $ap->[$_] >>= $sr;
#    $ap->[$_] |= ($ap->[$i] & $msk) << $sl;
    $ap->[$_] |= ($ap->[$i] << $sl) & $msk;
    $i++;
  }
  $ap->[$#$ap] >>= $sr;
}

# see the comments at "longshiftright" about the
# integration of calculations into the local subroutine
#
sub shiftright {
  my($ap,$n) = @_;
    longshiftright($ap,$n,$srindx2[$n],32 -$n);
}

#
# fast direct conversion of base power of 2 sets to base 2^32
#
sub bx1 {	# base 2,   1 bit  wide x32 = 32 bits - 111  32 1's 111111111111111
  my($ss,$d32p) = @_;
  unshift @$d32p, unpack('N1',pack('B32',$ss));
}

my %dna= ('AA', 0, 'AC', 1, 'AT', 2, 'AG', 3, 'CA', 4, 'CC', 5, 'CT', 6, 'CG', 7, 'TA', 8, 'TC', 9, 'TT', 10, 'TG', 11, 'GA', 12, 'GC', 13, 'GT', 14, 'GG', 15,
	  'Aa', 0, 'Ac', 1, 'At', 2, 'Ag', 3, 'Ca', 4, 'Cc', 5, 'Ct', 6, 'Cg', 7, 'Ta', 8, 'Tc', 9, 'Tt', 10, 'Tg', 11, 'Ga', 12, 'Gc', 13, 'Gt', 14, 'Gg', 15,
	  'aA', 0, 'aC', 1, 'aT', 2, 'aG', 3, 'cA', 4, 'cC', 5, 'cT', 6, 'cG', 7, 'tA', 8, 'tC', 9, 'tT', 10, 'tG', 11, 'gA', 12, 'gC', 13, 'gT', 14, 'gG', 15,
	  'aa', 0, 'ac', 1, 'at', 2, 'ag', 3, 'ca', 4, 'cc', 5, 'ct', 6, 'cg', 7, 'ta', 8, 'tc', 9, 'tt', 10, 'tg', 11, 'ga', 12, 'gc', 13, 'gt', 14, 'gg', 15,

);

# substr 4x faster than array lookup
#
sub bx2 {	# base 4,   2 bits wide x16 = 32 bits - 3333333333333333
  my($ss,$d32p) = @_;    
  my $bn = $dna{substr($ss,0,2)};	# 2 digits as a time => base 16
  $bn <<= 4;
  $bn += $dna{substr($ss,2,2)};
  $bn <<= 4;
  $bn += $dna{substr($ss,4,2)};
  $bn <<= 4;
  $bn += $dna{substr($ss,6,2)};
  $bn <<= 4;
  $bn += $dna{substr($ss,8,2)};
  $bn <<= 4;
  $bn += $dna{substr($ss,10,2)};
  $bn <<= 4;
  $bn += $dna{substr($ss,12,2)};
  $bn <<= 4;
  $bn += $dna{substr($ss,14,2)};
  unshift @$d32p, $bn;
}

sub bx3 {	# base 8,   3 bits wide x10 = 30 bits - 07777777777
  my($ss,$d32p) = @_;          
  unshift @$d32p, CORE::oct($ss) << 2;
  shiftright($d32p,2);
}

sub bx4 {	# base 16,  4 bits wide x8  = 32 bits - 0xffffffff
  my($ss,$d32p) = @_;          
  unshift @$d32p, CORE::hex($ss);
}

sub bx5 {	# base 32,  5 bits wide x6  = 30 bits - 555555
  my($ss,$d32p,$hsh) = @_;          
  my $bn = $hsh->{substr($ss,0,1)};
  $bn <<= 5;
  $bn += $hsh->{substr($ss,1,1)};
  $bn <<= 5;
  $bn += $hsh->{substr($ss,2,1)};
  $bn <<= 5;
  $bn += $hsh->{substr($ss,3,1)};
  $bn <<= 5;
  $bn += $hsh->{substr($ss,4,1)};
  $bn <<= 5;
  unshift @$d32p, ($bn += $hsh->{substr($ss,5,1)}) << 2;
  shiftright($d32p,2);
}

sub bx6 {	# base 64,  6 bits wide x5  = 30 bits - 66666
  my($ss,$d32p,$hsh) = @_;          
  my $bn = $hsh->{substr($ss,0,1)};
  $bn <<= 6;
  $bn += $hsh->{substr($ss,1,1)};
  $bn <<= 6;
  $bn += $hsh->{substr($ss,2,1)};
  $bn <<= 6;
  $bn += $hsh->{substr($ss,3,1)};
  $bn <<= 6;
  unshift @$d32p, ($bn += $hsh->{substr($ss,4,1)}) << 2;
  shiftright($d32p,2);
}

sub bx7 {	# base 128, 7 bits wide x4  = 28 bits - 7777
  my($ss,$d32p,$hsh) = @_;          
  my $bn = $hsh->{substr($ss,0,1)};
  $bn <<= 7;
  $bn += $hsh->{substr($ss,1,1)};
  $bn <<= 7;
  $bn += $hsh->{substr($ss,2,1)};
  $bn <<= 7;
  unshift @$d32p, ($bn += $hsh->{substr($ss,3,1)}) << 4;
  shiftright($d32p,4);
}

sub bx8 {	# base 256, 8 bits wide x4  = 32 bits - 8888
  my($ss,$d32p,$hsh) = @_;          
  my $bn = $hsh->{substr($ss,0,1)};
  $bn *= 256;
  $bn += $hsh->{substr($ss,1,1)};
  $bn *= 256;
  $bn += $hsh->{substr($ss,2,1)};
  $bn *= 256;
  unshift @$d32p, $bn += $hsh->{substr($ss,3,1)};
}

my @useFROMbaseShortcuts = ( 0,	# unused
	\&bx1,		# base 2,   1 bit  wide x32 = 32 bits - 111  32 1's  111111111111111
	\&bx2,		# base 4,   2 bits wide x16 = 32 bits - 3333333333333333
	\&bx3,		# base 8,   3 bits wide x10 = 30 bits - 07777777777
	\&bx4,		# base 16,  4 bits wide x8  = 32 bits - 0xffffffff
	\&bx5,		# base 32,  5 bits wide x6  = 30 bits - 555555
	\&bx6,		# base 64,  6 bits wide x5  = 30 bits - 66666
	\&bx7,		# base 128, 7 bits wide x4  = 28 bits - 7777
	\&bx8,	    # and base 256, 8 bits wide x4  = 32 bits - 8888
);

# 1) find number of digits of base that will fit in 2^32
# 2) pad msb's
# 3) substr digit groups and get value

sub useFROMbaseShortcuts {
  my $bc = shift;
  my($ary,$hsh,$base,$str) = @{$bc}{qw(from fhsh fbase nstr)};
  my $bp = int(log($base)/log(2) +0.5);
  my $len 	= length($str);
  return ($bp,[0]) unless $len;		# no value in zero length string

  my $shrink	= 32 % ($bp * $base);		# bits short of 16 bits

# convert any strings in standard convertable bases that are NOT standard strings to the standard
  my $basnam = ref $ary;
  my $padchar = $ary->[0];
  if ($base == 16) {				# should be hex
    if ($basnam !~ /HEX$/i) {
      $bc->{fHEX} = $bc->HEX() unless exists $bc->{fHEX};
      my @h = @{$bc->{fHEX}};
      $str =~ s/(.)/$h[$hsh->{$1}]/g;		# translate string to HEX
      $padchar = 0;
    }
  }
  elsif ($base == 8) {
    if ($basnam !~ /OCT$/i) {
      $bc->{foct} = $bc->ocT() unless exists $bc->{foct};
      my @o = @{$bc->{foct}};
      $str =~ s/(.)/$o[$hsh->{$1}]/g;
      $padchar = '0';
    }
  }
  elsif ($base == 4) {				# will map to hex
    if ($basnam !~ /dna$/i) {
      $bc->{fDNA} = $bc->DNA() unless exists $bc->{fDNA};
      my @d = @{$bc->{fDNA}};
      $str =~ s/(.)/$d[$hsh->{$1}]/g;
      $padchar = 'A';
    }
  }
  elsif ($base == 2) {			# will map to binary
    if ($basnam !~ /bin$/) {
      $bc->{fbin} = $bc->bin() unless exists $bc->{fbin};
      my @b = @{$bc->{fbin}};
      $str =~ s/(.)/$b[$hsh->{$1}]/g;
      $padchar = '0';
    }
  }

# digits per 32 bit register - $dpr
# $dpr = int(32 / $bp)	  =	32 / digit bit width
#
# number of digits to pad string so the last digit fits exactly in a 32 bit register
# $pad = digits_per_reg - (string_length % $dpr)
  my $dpr	= int (32 / $bp);
  my $pad 	= $dpr - ($len % $dpr);
  $pad = 0 if $pad == $dpr;
  if ($pad) {
    $str = ($padchar x $pad) . $str;		# pad string with zero value digit
  }

# number of iterations % digits/register
  $len += $pad;
  my $i = 0;
  my @d32;
  while ($i < $len) {
    #
    # base16 digit = sub bx[base power](string fragment ) 
    # where base power is the width of each nibble and
    # base is the symbol value width in bits

    $useFROMbaseShortcuts[$bp]->(substr($str,$i,$dpr),\@d32,$hsh);
    $i += $dpr;
  }
  while($#d32 && ! $d32[$#d32]) {		# waste leading zeros
    pop @d32;
  }
  $bc->{b32str} = \@d32;
}

# map non-standard user base to bitstream lookup
#
sub usrmap {
  my($to,$map) = @_;
  my %map;
  while (my($key,$val) = each %$map) {
    $map{$key} = $to->[$val];
  }
  \%map;
}  

sub useTObaseShortcuts {
  my $bc = shift;
  my($base,$b32p,$to) = @{$bc}{qw( tbase b32str to )};
  my $bp = int(log($base)/log(2) +0.5);		# base power
  my $L = @$b32p;
  my $packed = pack("N$L", reverse @{$b32p});
  ref($to) =~ /([^:]+)$/;			# extract to base name
  my $bname = $1;
  my $str;
  if ($bp == 1) {				# binary
    $L *= 32;
    ($str = unpack("B$L",$packed)) =~ s/^0+//;	# suppress leading zeros
    $str =~ s/(.)/$to->[$1]/g if $bname eq 'user';
  }
  elsif ($bp == 4) {				# hex / base 16
    $L *= 8;
    ($str = unpack("H$L",$packed)) =~ s/^0+//;	# suppress leading zeros
    $str =~ s/(.)/$to->[CORE::hex($1)]/g if $bname eq 'user';
  }
   else {					# the rest
    my $map;
    if ($bname eq 'user') {			# special map request
      unless (exists $bc->{tmap}) {
        $bc->{tmap} = usrmap($to,$xlt->[$bp]);	# cache the map for speed
      }
      $map = $bc->{tmap};
    }
    elsif ($bp == 3) {				# octal variant?
      $map = $xlt->[$bp];
    } else {
      $map = $xlt->[0]->{$bname};		# standard map
    }
    $L *= 32;
    (my $bits = unpack("B$L",$packed)) =~ s/^0+//;	# suppress leading zeros
#print "bp = $bp, BITS=\n$bits\n";
    my $len = length($bits);
    my $m = $len % $bp;				# pad to even multiple base power
#my $z = $m;
    if ($m) {
      $m = $bp - $m;
      $bits = ('0' x $m) . $bits;
      $len += $m;
    }
#print "len = $len, m_init = $z, m = $m, BITS PADDED\n$bits\n";
    $str = '';
    for (my $i = 0; $i < $len; $i += $bp) {
      $str .= $map->{substr($bits,$i,$bp)};
#print "MAPPED i=$i, str=$str\n";
    }
  }
  $str;
}

1;

__END__

=head1 NAME

Math::Base::Convert::Shortcuts - methods for converting powers of 2 bases

=head1 DESCRIPTION

This module contains two primary methods that convert bases that are exact
powers of 2 to and from base 2^32 faster than can be done by pure perl math.

=over 4

=item * $bc->useFROMbaseShortcuts

This method converts FROM an input base number to a long n*32 bit register

=item * $output = $bc->useTObaseShortcuts;

This method converts an n*32 bit registers TO an output base number.

=item * EXPORTS

None

=back

=head1 AUTHOR
   
Michael Robinton, michael@bizsystems.com

=head1 COPYRIGHT

Copyright 2012-2015, Michael Robinton

This program is free software; you may redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 
=cut

1;
