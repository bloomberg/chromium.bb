#!/usr/bin/perl

package Math::Base::Convert;

#use diagnostics;
use Carp;
use vars qw($VERSION @ISA @EXPORT_OK %EXPORT_TAGS @BASES $signedBase);

# @Bases, $signedBase	imported from Math::Base::Convert::Bases

require Exporter;
require Math::Base::Convert::Shortcuts;
require Math::Base::Convert::CalcPP;
require Math::Base::Convert::Bases;	# drag in BASES

@ISA = qw(
	Math::Base::Convert::Shortcuts
	Math::Base::Convert::CalcPP
	Exporter
);

$VERSION = do { my @r = (q$Revision: 0.11 $ =~ /\d+/g); sprintf "%d."."%02d" x $#r, @r };

@EXPORT_OK   = ( qw( cnv cnvpre cnvabs basemap ), @BASES );
%EXPORT_TAGS = (
	all	=> [@EXPORT_OK],
	base	=> [ 'basemap', @BASES ]
);

my $functions = join '', keys %{__PACKAGE__ .'::'};	# before 'strict'

use strict;

my $package = __PACKAGE__;
my $packageLen = length __PACKAGE__;
my $bs = $package .'::_bs::';	# indentify 'base sub'

my %num2sub = (
	2	=> &bin,
	4	=> &DNA,
	8	=> &ocT,
	10	=> &dec,
	16	=> &HEX,
	64	=> &m64
);

# return a hash map of the base array, including upper/lower case variants
#
sub basemap {
  shift if ref $_[0] eq $package;	# waste if method call
  my $base = validbase($_[0]);		# return array pointer
  ref($base) =~ /$bs(.+)/;		# sub name is $1
  if ($1 eq 'user') {			# if user array
    my $aryhsh = {};
    @{$aryhsh}{@$base} = (0..$#$base);
    return $aryhsh;
  }
  my @all = $functions =~ /$1/gi;	# get all matching sub names regardless of case
# names are strings
  no strict;
  my %aryhsh;
  foreach (@all) {
    $_ = $package->can($_);		# return sub ref
    $_ = &$_;				# array pointer
    foreach my $i (0..$#$_) {
      $aryhsh{$_->[$i]} = $i;		# map keys to index
    }
  }
  return \%aryhsh;
}
  
# check for internal base
sub validbase {
  my $base = shift;
  my $ref;
  if (($ref = ref $base)) {
    if ($ref eq 'ARRAY') {	# user supplied
      my @base = @{$base};
      my $len = @base;
      Carp::croak "base to short, < 2" unless $len > 1;
      Carp::croak "base to long, > 65535" unless $len < 65536;
      $base = bless \@base, $bs .'user';
      return bless $base, $bs . 'user';
    }
    elsif ($ref =~ /^$bs/) {	# internal base
      return $base;
    }
    else {
      $base = 'reference';
    }
  }
  elsif ($base =~ /\D/) {	# is a string
    my $rv = $package->can($base);
    return &$rv if $rv;
  } else {
    return $num2sub{$base} if exists $num2sub{$base};
  }
  Carp::croak "not a valid base: $base";
}

sub vet {
  my $class	= shift;
  my $from	= shift || '';
  my $to	= shift || '';

  $to	=~ s/\s+//g if $to && ! ref $to;	# strip white space
  $from	=~ s/\s+//g if $from && ! ref $from;

  unless ($from) {		# defaults if not defined
    $to   = &HEX;
    $from = &dec;
  }
  else {
    $from = validbase($from);
    unless ($to) {
      $to = &HEX;
    } else {
      $to = validbase($to);
    }
  }

# convert sub ref's to variables
#  $to			= &$to;
#  ($from, my $fhsh)	= &$from;

  my $prefix = ref $to;
  if ($prefix =~ /HEX$/i) {
    $prefix = '0x';
  }
  elsif ($prefix =~ /OCT$/i) {
    $prefix = '0';
  }
  elsif ($prefix =~ /bin$/) {
    $prefix = '0b';
  } else {
    $prefix = '';
  }

  bless {
	to	=> $to,
	tbase	=> scalar @$to,
	from	=> $from,
	fhsh	=> basemap($from),
	fbase	=> scalar @$from,
	prefix	=> $prefix
  }, $class;
}

sub new {
  my $proto     = shift;
  my $class     = ref $proto || $proto || $package;
  vet($class,@_);
}

sub _cnv {
  my $bc = shift;
  my $nstr;
  if (ref $bc && ref($bc) eq $package) {	# method call?
    $nstr = shift;			# yes, number to convert is next arg
  } else {
    $nstr = $bc;			# no, first arg is number to convert
    $bc = $package->new(@_);
  }
  return $nstr unless keys %$bc;	# if there really is no conversion
  $nstr = '' unless defined $nstr;

  my($from,$fbase,$fhsh) = @{$bc}{qw( from fbase fhsh )};

  my $ref = ref $from;
  if ($ref eq 'user' || $fbase > $signedBase) {	#  known, signed character sets?
    $bc->{sign} = '';			# no
  } else {				# yes
    $nstr =~ s/^([+-])//;			# strip sign
    $bc->{sign} = $1 && $1 eq '-' ? '-' : '';	# and save for possible restoration

    if ($ref =~ /(HEX)$/i) {
      $nstr =~ s/^0x//i;	# snip prefix, including typo's
    }
    elsif ($ref =~ /bin/i) {
      $nstr =~ s/^0b//i;	# snip prefix, including typo's
    }

    $nstr =~ s/^[$from->[0]]+//;		# snip leading zeros
  }

  my $fclass = join '', keys %$fhsh;
  if ($nstr =~ /[^\Q$fclass\E]/) {		# quote metacharacters
    $ref =~ /([^:]+)$/;
    Carp::croak "input character not in '$1'\nstring:\t$nstr\nbase:\t$fclass\n";
  }

  $bc->{nstr} = $nstr;
  $bc;
}

#
# Our internal multiply & divide = base 32
# Maximum digit length for a binary base = 32*ln(2)/ln(base)
# 0bnnnnnnnnnnn
# 0nnnnnnnnnnnn   
# 0xnnnnnnnnnnn
#

my %maxdlen = (# digits, key is base
	  2	=> 31,	# 2^1
	  4	=> 16,	# 2^2
	  8	=> 10,	# 2^3
	 16	=>  8,	# 2^4
	 32	=>  6,	# 2^5
	 64	=>  5,	# 2^6
	128	=>  4,	# 2^7
	256	=>  4	# 2^8
);

sub cnv {
  my @rv = &cnvpre;
  return @rv if wantarray;
  return ($rv[0] . $rv[2]);	# sign and string only
}

sub cnvabs {
  my @rv = &cnvpre;
  return @rv if wantarray;
  return $rv[2]			# string only
}

sub cnvpre {
  my $bc = &_cnv;
  return $bc unless ref $bc;
  my($from,$fbase,$to,$tbase,$sign,$prefix,$nstr) = @{$bc}{qw( from fbase to tbase sign prefix nstr)};

  my $slen  = length($nstr);
  my $tref  = ref($to);
  unless ($slen) {			# zero length input
    $nstr = $to->[0];			# return zero
  }
  elsif (lc $tref eq lc ref($from)) {# no base conversion
    if ($tref ne ref($from)) {		# convert case?
      if ($tref =~ /(?:DNA|HEX)/) {
	$nstr = uc $nstr;		# force upper case
      } else {
	$nstr = lc $nstr;		# or force lower case
      }
    }
  }
  else {				# convert

    my $fblen = length($fbase);
    if ($fbase & $fbase -1 ||		# from base is not power of 2
        $fblen > 256 ) {		# no shortcuts,...
      $bc->useFROMbaseto32wide;
    }

# if a large base and digit string will fit in a single 32 bit register
    elsif ( $fblen > 32 &&		# big base
#	    exists $maxdlen{$fbase} &&	# has to exist
	  ! $slen > $maxdlen{$fbase}) {
      $bc->useFROMbaseto32wide;		# CalcPP is faster
    }
    else {				# shortcuts faster for big numbers
      $bc->useFROMbaseShortcuts;
    }

  ################################
  # input converted to base 2^32 #
  ################################

    if ($tbase & $tbase -1 ||		# from base is not power of 2
        $tbase > 256 ) {			# no shortcuts,...
      $nstr = $bc->use32wideTObase;
    }
# if big base  and digit string fits in a single 32 bit register
    elsif ( $tbase > 32 &&  @{$bc->{b32str}} == 1) {
      $nstr = $bc->use32wideTObase;	# CalcPP is faster
    }
    else {
      $nstr = $bc->useTObaseShortcuts;	# shortcuts faster for big numbers
    }
  } # end convert

  $nstr = $to->[0] unless length($nstr);
  return ($sign,$prefix,$nstr) if wantarray;
  if (#$prefix ne '' &&		# 0, 0x, 0b
      $tbase <= $signedBase &&		# base in signed set
      $tref ne 'user' ) {		# base standard
    return ($sign . $prefix . $nstr);
  }
  return ($prefix . $nstr);
}
    
sub _cnvtst {
  my $bc = &_cnv;
  return $bc unless ref $bc;
  $bc->useFROMbaseto32wide;
  return $bc->use32wideTObase unless wantarray;
  return (@{$bc}{qw( sign prefix )},$bc->use32wideTObase);
}

=head1 NAME

Math::Base::Convert - very fast base to base conversion

=head1 SYNOPSIS

=head2 As a function

  use Math::Base::Convert qw( :all )
  use Math::Base::Convert qw( 

	cnv
	cnvabs
	cnvpre
	basemap

			# comments
	bin		base 2 0,1
	dna		base 4 lower case dna
	DNA		base 4 upper case DNA
	oct		base 8 octal
	dec		base 10 decimal
	hex		base 16 lower case hex
	HEX		base 16 upper case HEX
	b62		base 62
	b64		base 64 month:C:12 day:V:31
	m64		base 64 0-63 from MIME::Base64
	iru		base 64 P10 protocol - IRCu daemon
	url		base 64 url with no %2B %2F expansion of + - /
	rex		base 64 regular expression variant
	id0		base 64 IDentifier style 0
	id1		base 64 IDentifier style 1
	xnt		base 64 XML Name Tokens (Nmtoken)
	xid		base 64 XML identifiers (Name)
	b85		base 85 RFC 1924 for IPv6 addresses
	ascii		base 96 7 bit printible 0x20 - 0x7F
  );

  my $converted = cnv($number,optionalFROM,optionalTO);
  my $basemap = basmap(base);

=head2 As a method:

  use Math::Base::Convert;
  use Math::Base::Convert qw(:base);

  my $bc = new Math::Base::Convert(optionalFROM,optionalTO);
  my $converted = $bc->cnv($number);
  my $basemap = $bc->basemap(base);

=head1 DESCRIPTION

This module provides fast functions and methods to convert between arbitrary number bases
from 2 (binary) thru 65535.

This module is pure Perl, has no external dependencies, and is backward compatible
with old versions of Perl 5.

=head1 PREFERRED USE

Setting up the conversion parameters, context and error checking consume a significant portion of the execution time of a 
B<single> base conversion. These operations are performed each time B<cnv> is called as a function.

Using method calls eliminates a large portion of this overhead and will improve performance for 
repetitive conversions. See the benchmarks sub-directory in this distribution.

=head1 BUILT IN NUMBER SETS

Number set variants courtesy of the authors of Math::Base:Cnv and
Math::BaseConvert.

The functions below return a reference to an array

  $arrayref 	= function;

  bin => ['0', '1']				  # binary
  dna => ['a','t','c','g']			  # lc dna
  DNA => ['A','T','C','G'],	{default}	  # uc DNA
  oct => ['0'..'7']				  # octal
  dec => ['0'..'9']				  # decimal
  hex => ['0'..'9', 'a'..'f']			  # lc hex
  HEX => ['0'..'9', 'A'..'F']	{default}	  # uc HEX
  b62 => ['0'..'9', 'a'..'z', 'A'..'Z']		  # base 62
  b64 => ['0'..'9', 'A'..'Z', 'a'..'z', '.', '_'] # m:C:12 d:V:31
  m64 => ['A'..'Z', 'a'..'z', '0'..'9', '+', '/'] # MIMI::Base64
  iru => ['A'..'Z', 'a'..'z', '0'..'9', '[', ']'] # P10 - IRCu
  url => ['A'..'Z', 'a'..'z', '0'..'9', '*', '-'] # url no %2B %2F
  rex => ['A'..'Z', 'a'..'z', '0'..'9', '!', '-'] # regex variant
  id0 => ['A'..'Z', 'a'..'z', '0'..'9', '_', '-'] # ID 0
  id1 => ['A'..'Z', 'a'..'z', '0'..'9', '.', '_'] # ID 1
  xnt => ['A'..'Z', 'a'..'z', '0'..'9', '.', '-'] # XML (Nmtoken)
  xid => ['A'..'Z', 'a'..'z', '0'..'9', '_', ':'] # XML (Name)
  b85 => ['0'..'9', 'A'..'Z', 'a'..'z', '!', '#', # RFC 1924
	  '$', '%', '&', '(', ')', '*', '+', '-', 
	  ';', '<', '=', '>', '?', '@', '^', '_', 
	  '', '{', '|', '}', '~']
  An arbitrary base 96 composed of printable 7 bit ascii
  from 0x20 (space) through 0x7F (tilde ~)
  ascii => [
	' ','!','"','#','$','%','&',"'",'(',')',
	'*','+',',','-','.','/',
	'0','1','2','3','4','5','6','7','8','9',
	':',';','<','=','>','?','@',
	'A','B','C','D','E','F','G','H','I','J','K','L','M',
	'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
	'[','\',']','^','_','`',
	'a','b','c','d','e','f','g','h','i','j','k','l','m',
	'n','o','p','q','r','s','t','u','v','w','x','y','z',
	'{','|','}','~']

  NOTE: Clean text with =~ s/\s+/ /; before applying to ascii

=head1 USAGE

=over 4

=item * $converted = cnv($number,[from],[to])

SCALAR context: array context covered later in this document.

To preserve similarity to other similar base conversion modules, B<cnv>
returns the converted number string with SIGN if both the input and output 
base strings are in known signed set of bases in this module.

In the case of binary, octal, hex, all leading base designator strings such as
'0b','0', '0x' are automatically stripped from the input. Base designator
strings are NOT applied to the output.

The context of base FROM and TO is optional and flexible.

Unconditional conversion from decimal to HEX [upper case]

	$converted = cnv($number);

Example conversion from octal to default HEX [upper case] with different
context for the 'octal' designator.

  base as a number
	$converted = cnv($number,8);

  base as a function	(imported)
	$converted = cnv($number,oct);

  base as text
	$converted = convbase($number,'oct');

Conversion to/from arbitrary bases i.e.

  $converted = cnv($number); # dec -> hex (default)
  $converted = cnv($number,oct);	# oct to HEX
  $converted = cnv($number,10,HEX);	# dec to uc HEX
  $converted = cnv($number,10,hex);	# dec to lc hex
  $converted = cnv($number,dec,hex);#    same

	pointer notation
  $converted = cnv($number, oct => dec);

  $converted = cnv($number,10 => 23); # dec to base23
  $converted = cnv($number,23 => 5);  # b23 to base5
  etc...

=item * $bc = new Math::Base::Convert([from],[to]);

This method has the same usage and syntax for FROM and TO as B<cnv> above.

Setup for unconditional conversion from HEX to decimal

	$bc = new Math::Base::Convert();

Example conversion from octal to decimal

  base number
	$bc = new Math::Base::Convert(8);

  base function	(imported)
	$bc = new Math::Base::Convert(oct);

  base text
	$bc = new Math::Base::Convert('oct')

The number conversion for any of the above:

NOTE: iterative conversions using a method pointer are ALWAYS faster than
calling B<cnv> as a function.

	$converted = $bc->cnv($number);

=item * $converted = cnvpre($number,[from],[to])

Same as B<cnv> except that base descriptor PREfixes are applied to B<binary>,
B<octal>, and B<hexadecimal> output strings.

=item * $converted = cnvabs($number,[from],[to])

Same as B<cnv> except that the ABSolute value of the number string is
returned without SIGN is returned. i.e. just the raw string.

=item * ($sign,$prefix,$string) = cnv($number,[$from,[$to]])

=item * ($sign,$prefix,$string) = cnv($number,[$from,[$to]])

=item * ($sign,$prefix,$string) = cnv($number,[$from,[$to]])

ARRAY context:

All three functions return the same items in array context.

  sign		the sign of the input number string

  prefix	the prefix which would be applied to output

  string	the raw output string

=item * $basemap = basemap(base);

=item * $basemap = $bc->basemap(base);

This function / method returns a pointer to a hash that maps the keys of a base to its
numeric value for base conversion. It accepts B<base> in any of the forms
described for B<cnv>.

The return basemap includes upper and lower case variants of the the number
base in cases such as B<hex> where upper and lower case a..f, A..F map to
the same numeric value for base conversion.

  i.e. $hex_ptr = {
	0  => 0,
	1  => 1,
	2  => 2,
	3  => 3,
	4  => 4,
	5  => 5,
	6  => 6,
	7  => 7,
	8  => 8,
	9  => 9,
	A  => 10,
	B  => 11,
	C  => 12,
	D  => 13,
	E  => 14,
	F  => 15,
	a  => 10,
	b  => 11,
	c  => 12,
	d  => 13,
	e  => 14,
	f  => 15
  };

=back

=head1 BENCHMARKS

Math::Base::Convert includes 2 development and one real world benchmark
sequences included in the test suite. Benchmark results for a 500mhz system
can be found in the 'benchmarks' source directory.

  make test BENCHMARK=1

Provides comparison data for bi-directional conversion of an ascending
series of number strings in all base powers. The test sequence contains
number strings that go from a a single 32 bit register to several. Tested
bases are:   (note: b32, b128, b256 not useful and are for testing only)

    base 2    4    8    16   32   64   85   128   256
	bin, dna, oct, hex, b32, b64, b85, b128, b256

Conversions are performed FROM all bases TO decimal and are repeated in the
opposing direction FROM decimal TO all bases.

Benchmark 1 results indicate the Math::Base::Convert typically runs
significantly faster ( 10x to 100x) than Math::BigInt based
implementations used in similar modules.

  make test BENCHMARK=2

Provides comparison data for the frontend and backend converters in
Math::Base::Convert's CalcPP and Shortcuts packages, and Math::Bigint
conversions if it is present on the system under test.

  make test BENCHMARK=3

Checks the relative timing of short and long number string conversions. FROM
a base number to n*32 bit register and TO a base number from an n*32 bit
register set.

i.e. strings that convert to and from 1, 2, 3... etc.. 32 bit registers

=head1 DEPENDENCIES

	none

	Math::BigInt is conditionally used in
	the test suite but is not a requirement

=head1 EXPORT_OK

Conditional EXPORT functions

	cnv
	cnvabs
	cnvpre
	basemap
	bin
	oct
	dec
	heX
	HEX
	b62
	b64
	m64
	iru
	url
	rex
	id0
	id1
	xnt
	xid
	b85
	ascii

=head1 EXPORT_TAGS

Conditional EXPORT function groups

	:all	=> all of above
	:base	=> all except 'cnv,cnvabs,cnvpre'

=head1 ACKNOWLEDGEMENTS

This module was inspired by Math::BaseConvert maintained by Shane Warden
<chromatic@cpan.org> and forked from Math::BaseCnv, both authored by Pip
Stuart <Pip@CPAN.Org>


=head1 AUTHOR

Michael Robinton, <miker@cpan.org>

=head1 COPYRIGHT

Copyright 2012-2015, Michael Robinton

This program is free software; you may redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

=cut

1;
