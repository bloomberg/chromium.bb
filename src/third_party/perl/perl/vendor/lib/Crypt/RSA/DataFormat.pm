package Crypt::RSA::DataFormat; 
use strict;
use warnings;

## Crypt::RSA::DataFormat -- Functions for converting, shaping and 
##                           creating and reporting data formats.
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.

use vars qw(@ISA);
use Math::BigInt try => 'GMP, Pari';
use Math::Prime::Util qw/random_bytes/;
use Digest::SHA qw(sha1);
use Carp;

use base qw(Exporter);
our @EXPORT_OK = qw(i2osp os2ip h2osp octet_xor octet_len bitsize 
                    generate_random_octet mgf1 steak);


sub i2osp {
    my ($num, $l) = @_;
    $num = 0 if !defined $num;
    $l   = 0 if !defined $l;
    my $result;

    if (ref($num) ne 'Math::BigInt' && $num <= ~0) {
      $result = '';
      do {
        $result = chr($num & 0xFF) . $result;
        $num >>= 8;
      } while $num;
    } else {
      $num = Math::BigInt->new("$num") unless ref($num) eq 'Math::BigInt';
      my $hex = $num->as_hex;
      # Remove the 0x and any leading zeros (shouldn't be any)
      $hex =~ s/^0x0*//;
      # Add a leading zero if needed to line up bytes correctly.
      substr($hex, 0, 0, '0') if length($hex) % 2;
      # Pack hex string into a octet string.
      $result = pack("H*", $hex);
    }

    if ($l) {
      my $rlen = length($result);
      # Return undef if too large to fit
      return if $l < $rlen;
      # Zero pad the front if they want a longer string
      substr( $result, 0, 0, chr(0) x ($l-$rlen) ) if $rlen < $l;
    }
    return $result;
}


sub os2ip {
    my $string = shift;
    my($hex) = unpack('H*', $string);
    return Math::BigInt->new("0x$hex");
}


sub h2osp { 
    my $hex = shift;
    $hex =~ s/\s//g;
    $hex =~ s/^0x0*//;
    # Add a leading zero if needed to line up bytes correctly.
    substr($hex, 0, 0, '0') if length($hex) % 2;
    # Pack into a string.
    my $result = pack("H*", $hex);
    return $result;
}


sub generate_random_octet {
  my $l = shift;   # Ignore the strength parameter, if any
  return random_bytes($l);
}


sub bitsize {
  my $n = shift;
  my $bits = 0;
  if (ref($n) eq 'Math::BigInt') {
    $bits = length($n->as_bin) - 2;
  } else {
    while ($n) { $bits++; $n >>= 1; }
  }
  $bits;
}


sub octet_len { 
  return int( (bitsize(shift) + 7) / 8 );
}


# This could be made even faster doing 4 bytes at a time
sub octet_xor {
    my ($a, $b) = @_;

    # Ensure length($a) >= length($b)
    ($a, $b) = ($b, $a) if length($b) > length($a);
    my $alen = length($a);
    # Prepend null bytes to the beginning of $b to make the lengths equal
    substr($b, 0, 0, chr(0) x ($alen-length($b))) if $alen > length($b);

    # xor all bytes
    my $r = '';
    $r .= chr( ord(substr($a,$_,1)) ^ ord(substr($b,$_,1)) ) for 0 .. $alen-1;
    return $r;
}


# http://rfc-ref.org/RFC-TEXTS/3447/chapter11.html
sub mgf1 {
    my ($seed, $l) = @_;
    my $hlen = 20;  # SHA-1 is 160 bits
    my $imax = int(($l + $hlen - 1) / $hlen) - 1;
    my $T = "";
    foreach my $i (0 .. $imax) {
      $T .= sha1($seed . i2osp ($i, 4));
    }
    my ($output) = unpack "a$l", $T;
    return $output;
}


sub steak { 
    my ($text, $blocksize) = @_; 
    my $textsize = length($text);
    my $chunkcount = $textsize % $blocksize 
        ? int($textsize/$blocksize) + 1 : $textsize/$blocksize;
    my @segments = unpack "a$blocksize"x$chunkcount, $text;
    return @segments;
}

1;

=head1 NAME

Crypt::RSA::DataFormat - Data creation, conversion and reporting primitives.

=head1 DESCRIPTION

This module implements several data creation, conversion and reporting
primitives used throughout the Crypt::RSA implementation. Primitives are
available as exportable functions.

=head1 FUNCTIONS

=over 4

=item B<i2osp> Integer, Length

Integer To Octet String Primitive. Converts an integer into its
equivalent octet string representation of length B<Length>. If
necessary, the resulting string is prefixed with nulls. If
B<Length> is not provided, returns an octet string of shortest
possible length.

=item B<h2osp> Hex String

Hex To Octet String Primitive. Converts a I<hex string> into its
equivalent octet string representation and returns an octet
string of shortest possible length. The hex string is not
prefixed with C<0x>, etc.

=item B<os2ip> String

Octet String to Integer Primitive. Converts an octet string into its
equivalent integer representation.

=item B<generate_random_octet> Length

Generates a random octet string of length B<Length>.

=item B<bitsize> Integer

Returns the length of the B<Integer> in bits.

=item B<octet_len> Integer

Returns the octet length of the integer. If the length is not a whole
number, the fractional part is dropped to make it whole.

=item B<octet_xor> String1, String2

Returns the result of B<String1> XOR B<String2>.

=item B<steak> String, Length 

Returns an array of segments of length B<Length> from B<String>. The final
segment can be smaller than B<Length>.

=back

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=cut


