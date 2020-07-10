package Math::Prime::Util::ChaCha;
use strict;
use warnings;
use Carp qw/carp croak confess/;

BEGIN {
  $Math::Prime::Util::ChaCha::AUTHORITY = 'cpan:DANAJ';
  $Math::Prime::Util::ChaCha::VERSION = '0.73';
}

###############################################################################
# Begin ChaCha core, reference RFC 7539
# with change to make blockcount/nonce be 64/64 from 32/96
# Dana Jacobsen, 9 Apr 2017

BEGIN {
  use constant ROUNDS => 20;
  use constant BUFSZ  => 1024;
  use constant BITS   => (~0 == 4294967295) ? 32 : 64;
}

#  State is:
#       cccccccc  cccccccc  cccccccc  cccccccc
#       kkkkkkkk  kkkkkkkk  kkkkkkkk  kkkkkkkk
#       kkkkkkkk  kkkkkkkk  kkkkkkkk  kkkkkkkk
#       bbbbbbbb  nnnnnnnn  nnnnnnnn  nnnnnnnn
#
#     c=constant k=key b=blockcount n=nonce

# We have to take care with 32-bit Perl so it sticks with integers.
# Unfortunately the pragma "use integer" means signed integer so
# it ruins right shifts.  We also must ensure we save as unsigned.

sub _core {
  my($j, $blocks) = @_;
  my $ks = '';
  $blocks = 1 unless defined $blocks;

  while ($blocks-- > 0) {
    my($x0,$x1,$x2,$x3,$x4,$x5,$x6,$x7,$x8,$x9,$x10,$x11,$x12,$x13,$x14,$x15) = @$j;
    for (1 .. ROUNDS/2) {
      use integer;
      if (BITS == 64) {
        $x0 =($x0 +$x4 )&0xFFFFFFFF; $x12^=$x0 ; $x12=(($x12<<16)|($x12>>16))&0xFFFFFFFF;
        $x8 =($x8 +$x12)&0xFFFFFFFF; $x4 ^=$x8 ; $x4 =(($x4 <<12)|($x4 >>20))&0xFFFFFFFF;
        $x0 =($x0 +$x4 )&0xFFFFFFFF; $x12^=$x0 ; $x12=(($x12<< 8)|($x12>>24))&0xFFFFFFFF;
        $x8 =($x8 +$x12)&0xFFFFFFFF; $x4 ^=$x8 ; $x4 =(($x4 << 7)|($x4 >>25))&0xFFFFFFFF;
        $x1 =($x1 +$x5 )&0xFFFFFFFF; $x13^=$x1 ; $x13=(($x13<<16)|($x13>>16))&0xFFFFFFFF;
        $x9 =($x9 +$x13)&0xFFFFFFFF; $x5 ^=$x9 ; $x5 =(($x5 <<12)|($x5 >>20))&0xFFFFFFFF;
        $x1 =($x1 +$x5 )&0xFFFFFFFF; $x13^=$x1 ; $x13=(($x13<< 8)|($x13>>24))&0xFFFFFFFF;
        $x9 =($x9 +$x13)&0xFFFFFFFF; $x5 ^=$x9 ; $x5 =(($x5 << 7)|($x5 >>25))&0xFFFFFFFF;
        $x2 =($x2 +$x6 )&0xFFFFFFFF; $x14^=$x2 ; $x14=(($x14<<16)|($x14>>16))&0xFFFFFFFF;
        $x10=($x10+$x14)&0xFFFFFFFF; $x6 ^=$x10; $x6 =(($x6 <<12)|($x6 >>20))&0xFFFFFFFF;
        $x2 =($x2 +$x6 )&0xFFFFFFFF; $x14^=$x2 ; $x14=(($x14<< 8)|($x14>>24))&0xFFFFFFFF;
        $x10=($x10+$x14)&0xFFFFFFFF; $x6 ^=$x10; $x6 =(($x6 << 7)|($x6 >>25))&0xFFFFFFFF;
        $x3 =($x3 +$x7 )&0xFFFFFFFF; $x15^=$x3 ; $x15=(($x15<<16)|($x15>>16))&0xFFFFFFFF;
        $x11=($x11+$x15)&0xFFFFFFFF; $x7 ^=$x11; $x7 =(($x7 <<12)|($x7 >>20))&0xFFFFFFFF;
        $x3 =($x3 +$x7 )&0xFFFFFFFF; $x15^=$x3 ; $x15=(($x15<< 8)|($x15>>24))&0xFFFFFFFF;
        $x11=($x11+$x15)&0xFFFFFFFF; $x7 ^=$x11; $x7 =(($x7 << 7)|($x7 >>25))&0xFFFFFFFF;
        $x0 =($x0 +$x5 )&0xFFFFFFFF; $x15^=$x0 ; $x15=(($x15<<16)|($x15>>16))&0xFFFFFFFF;
        $x10=($x10+$x15)&0xFFFFFFFF; $x5 ^=$x10; $x5 =(($x5 <<12)|($x5 >>20))&0xFFFFFFFF;
        $x0 =($x0 +$x5 )&0xFFFFFFFF; $x15^=$x0 ; $x15=(($x15<< 8)|($x15>>24))&0xFFFFFFFF;
        $x10=($x10+$x15)&0xFFFFFFFF; $x5 ^=$x10; $x5 =(($x5 << 7)|($x5 >>25))&0xFFFFFFFF;
        $x1 =($x1 +$x6 )&0xFFFFFFFF; $x12^=$x1 ; $x12=(($x12<<16)|($x12>>16))&0xFFFFFFFF;
        $x11=($x11+$x12)&0xFFFFFFFF; $x6 ^=$x11; $x6 =(($x6 <<12)|($x6 >>20))&0xFFFFFFFF;
        $x1 =($x1 +$x6 )&0xFFFFFFFF; $x12^=$x1 ; $x12=(($x12<< 8)|($x12>>24))&0xFFFFFFFF;
        $x11=($x11+$x12)&0xFFFFFFFF; $x6 ^=$x11; $x6 =(($x6 << 7)|($x6 >>25))&0xFFFFFFFF;
        $x2 =($x2 +$x7 )&0xFFFFFFFF; $x13^=$x2 ; $x13=(($x13<<16)|($x13>>16))&0xFFFFFFFF;
        $x8 =($x8 +$x13)&0xFFFFFFFF; $x7 ^=$x8 ; $x7 =(($x7 <<12)|($x7 >>20))&0xFFFFFFFF;
        $x2 =($x2 +$x7 )&0xFFFFFFFF; $x13^=$x2 ; $x13=(($x13<< 8)|($x13>>24))&0xFFFFFFFF;
        $x8 =($x8 +$x13)&0xFFFFFFFF; $x7 ^=$x8 ; $x7 =(($x7 << 7)|($x7 >>25))&0xFFFFFFFF;
        $x3 =($x3 +$x4 )&0xFFFFFFFF; $x14^=$x3 ; $x14=(($x14<<16)|($x14>>16))&0xFFFFFFFF;
        $x9 =($x9 +$x14)&0xFFFFFFFF; $x4 ^=$x9 ; $x4 =(($x4 <<12)|($x4 >>20))&0xFFFFFFFF;
        $x3 =($x3 +$x4 )&0xFFFFFFFF; $x14^=$x3 ; $x14=(($x14<< 8)|($x14>>24))&0xFFFFFFFF;
        $x9 =($x9 +$x14)&0xFFFFFFFF; $x4 ^=$x9 ; $x4 =(($x4 << 7)|($x4 >>25))&0xFFFFFFFF;
      } else { # 32-bit
        $x0 +=$x4 ; $x12^=$x0 ; $x12=($x12<<16)|(($x12>>16)& 0xFFFF);
        $x8 +=$x12; $x4 ^=$x8 ; $x4 =($x4 <<12)|(($x4 >>20)& 0xFFF);
        $x0 +=$x4 ; $x12^=$x0 ; $x12=($x12<< 8)|(($x12>>24)& 0xFF);
        $x8 +=$x12; $x4 ^=$x8 ; $x4 =($x4 << 7)|(($x4 >>25)& 0x7F);
        $x1 +=$x5 ; $x13^=$x1 ; $x13=($x13<<16)|(($x13>>16)& 0xFFFF);
        $x9 +=$x13; $x5 ^=$x9 ; $x5 =($x5 <<12)|(($x5 >>20)& 0xFFF);
        $x1 +=$x5 ; $x13^=$x1 ; $x13=($x13<< 8)|(($x13>>24)& 0xFF);
        $x9 +=$x13; $x5 ^=$x9 ; $x5 =($x5 << 7)|(($x5 >>25)& 0x7F);
        $x2 +=$x6 ; $x14^=$x2 ; $x14=($x14<<16)|(($x14>>16)& 0xFFFF);
        $x10+=$x14; $x6 ^=$x10; $x6 =($x6 <<12)|(($x6 >>20)& 0xFFF);
        $x2 +=$x6 ; $x14^=$x2 ; $x14=($x14<< 8)|(($x14>>24)& 0xFF);
        $x10+=$x14; $x6 ^=$x10; $x6 =($x6 << 7)|(($x6 >>25)& 0x7F);
        $x3 +=$x7 ; $x15^=$x3 ; $x15=($x15<<16)|(($x15>>16)& 0xFFFF);
        $x11+=$x15; $x7 ^=$x11; $x7 =($x7 <<12)|(($x7 >>20)& 0xFFF);
        $x3 +=$x7 ; $x15^=$x3 ; $x15=($x15<< 8)|(($x15>>24)& 0xFF);
        $x11+=$x15; $x7 ^=$x11; $x7 =($x7 << 7)|(($x7 >>25)& 0x7F);
        $x0 +=$x5 ; $x15^=$x0 ; $x15=($x15<<16)|(($x15>>16)& 0xFFFF);
        $x10+=$x15; $x5 ^=$x10; $x5 =($x5 <<12)|(($x5 >>20)& 0xFFF);
        $x0 +=$x5 ; $x15^=$x0 ; $x15=($x15<< 8)|(($x15>>24)& 0xFF);
        $x10+=$x15; $x5 ^=$x10; $x5 =($x5 << 7)|(($x5 >>25)& 0x7F);
        $x1 +=$x6 ; $x12^=$x1 ; $x12=($x12<<16)|(($x12>>16)& 0xFFFF);
        $x11+=$x12; $x6 ^=$x11; $x6 =($x6 <<12)|(($x6 >>20)& 0xFFF);
        $x1 +=$x6 ; $x12^=$x1 ; $x12=($x12<< 8)|(($x12>>24)& 0xFF);
        $x11+=$x12; $x6 ^=$x11; $x6 =($x6 << 7)|(($x6 >>25)& 0x7F);
        $x2 +=$x7 ; $x13^=$x2 ; $x13=($x13<<16)|(($x13>>16)& 0xFFFF);
        $x8 +=$x13; $x7 ^=$x8 ; $x7 =($x7 <<12)|(($x7 >>20)& 0xFFF);
        $x2 +=$x7 ; $x13^=$x2 ; $x13=($x13<< 8)|(($x13>>24)& 0xFF);
        $x8 +=$x13; $x7 ^=$x8 ; $x7 =($x7 << 7)|(($x7 >>25)& 0x7F);
        $x3 +=$x4 ; $x14^=$x3 ; $x14=($x14<<16)|(($x14>>16)& 0xFFFF);
        $x9 +=$x14; $x4 ^=$x9 ; $x4 =($x4 <<12)|(($x4 >>20)& 0xFFF);
        $x3 +=$x4 ; $x14^=$x3 ; $x14=($x14<< 8)|(($x14>>24)& 0xFF);
        $x9 +=$x14; $x4 ^=$x9 ; $x4 =($x4 << 7)|(($x4 >>25)& 0x7F);
      }
    }
    $ks .= pack("V16",$x0 +$j->[ 0],$x1 +$j->[ 1],$x2 +$j->[ 2],$x3 +$j->[ 3],
                      $x4 +$j->[ 4],$x5 +$j->[ 5],$x6 +$j->[ 6],$x7 +$j->[ 7],
                      $x8 +$j->[ 8],$x9 +$j->[ 9],$x10+$j->[10],$x11+$j->[11],
                      $x12+$j->[12],$x13+$j->[13],$x14+$j->[14],$x15+$j->[15]);
    if (++$j->[12] > 4294967295) {
      $j->[12] = 0;
      $j->[13]++;
    }
  }
  $ks;
}
sub _test_core {
  return unless ROUNDS == 20;
  my $init_state = '617078653320646e79622d326b20657403020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c00000001090000004a00000000000000';
  my @state = map { hex("0x$_") } unpack "a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8", $init_state;
  my $instr = join("",map { sprintf("%08x",$_) } @state);
  die "Block function fail test 2.3.2 input" unless $instr eq '617078653320646e79622d326b20657403020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c00000001090000004a00000000000000';
  my @out = unpack("V16", _core(\@state));
  my $outstr = join("",map { sprintf("%08x",$_) } @out);
  #printf "  %08x  %08x  %08x  %08x\n  %08x  %08x  %08x  %08x\n  %08x  %08x  %08x  %08x\n  %08x  %08x  %08x  %08x\n", @state;
  die "Block function fail test 2.3.2 output" unless $outstr eq 'e4e7f11015593bd11fdd0f50c47120a3c7f4d1c70368c0339aaa22044e6cd4c3466482d209aa9f0705d7c214a2028bd9d19c12b5b94e16dee883d0cb4e3c50a2';
}
_test_core();

# Returns integral number of 64-byte blocks.
sub _keystream {
  my($nbytes, $rstate) = @_;
  croak "Keystream invalid state" unless scalar(@$rstate) == 16;
  _core($rstate, ($nbytes+63) >> 6);
}
sub _test_keystream {
  return unless ROUNDS == 20;
  my $init_state = '617078653320646e79622d326b20657403020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c00000001000000004a00000000000000';
  my @state = map { hex("0x$_") } unpack "a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8", $init_state;
  my $instr = join("",map { sprintf("%08x",$_) } @state);
  die "Block function fail test 2.4.2 input" unless $instr eq '617078653320646e79622d326b20657403020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c00000001000000004a00000000000000';
  my $keystream = _keystream(114, \@state);
  # Verify new state
  my $outstr = join("",map { sprintf("%08x",$_) } @state);
  die "Block function fail test 2.4.2 output" unless $outstr eq '617078653320646e79622d326b20657403020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c00000003000000004a00000000000000';
  my $ksstr = unpack("H*",$keystream);
  die "Block function fail test 2.4.2 keystream" unless substr($ksstr,0,2*114) eq '224f51f3401bd9e12fde276fb8631ded8c131f823d2c06e27e4fcaec9ef3cf788a3b0aa372600a92b57974cded2b9334794cba40c63e34cdea212c4cf07d41b769a6749f3f630f4122cafe28ec4dc47e26d4346d70b98c73f3e9c53ac40c5945398b6eda1a832c89c167eacd901d7e2bf363';
}
_test_keystream();

# End ChaCha core
###############################################################################

# Simple PRNG used to fill small seeds
sub _prng_next {
  my($s) = @_;
  my $word;
  my $oldstate = $s->[0];
  if (BITS == 64) {
    $s->[0] = ($s->[0] * 747796405 + $s->[1]) & 0xFFFFFFFF;
    $word = ((($oldstate >> (($oldstate >> 28) + 4)) ^ $oldstate) * 277803737) & 0xFFFFFFFF;
  } else {
    { use integer; $s->[0] = unpack("L",pack("L", $s->[0] * 747796405 + $s->[1] )); }
    $word = (($oldstate >> (($oldstate >> 28) + 4)) ^ $oldstate) & 0xFFFFFFFF;
    { use integer; $word = unpack("L",pack("L", $word * 277803737)); }
  }
  ($word >> 22) ^ $word;
}
sub _prng_new {
  my($a,$b,$c,$d) = @_;
  my @s = (0, (($b << 1) | 1) & 0xFFFFFFFF);
  _prng_next(\@s);
  $s[0] = ($s[0] + $a) & 0xFFFFFFFF;
  _prng_next(\@s);
  $s[0] = ($s[0] ^ $c) & 0xFFFFFFFF;
  _prng_next(\@s);
  $s[0] = ($s[0] ^ $d) & 0xFFFFFFFF;
  _prng_next(\@s);
  \@s;
}
###############################################################################

# These variables are not accessible outside this file by standard means.
{
  my $_goodseed;     # Did we get a long seed
  my $_state;        # the cipher state.  40 bytes user data, 64 total.
  my $_str;          # buffered to-be-sent output.

  sub _is_csprng_well_seeded { $_goodseed }

  sub csrand {
    my($seed) = @_;
    $_goodseed = length($seed) >= 16;
    while (length($seed) % 4) { $seed .= pack("C",0); }  # zero pad end word
    my @seed = unpack("V*",substr($seed,0,40));
    # If not enough data, fill rest using simple RNG
    if ($#seed < 9) {
      my $rng = _prng_new(map { $_ <= $#seed ? $seed[$_] : 0 } 0..3);
      push @seed, _prng_next($rng) while $#seed < 9;
    }
    croak "Seed count failure" unless $#seed == 9;
    $_state = [0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
               @seed[0..7],
               0, 0, @seed[8..9]];
    $_str = '';
  }
  sub srand {
    my $seed = shift;
    $seed = CORE::rand unless defined $seed;
    if ($seed <= 4294967295) { csrand(pack("V",$seed)); }
    else                     { csrand(pack("V2",$seed,$seed>>32)); }
    $seed;
  }
  sub irand {
    $_str .= _keystream(BUFSZ,$_state) if length($_str) < 4;
    return unpack("V",substr($_str, 0, 4, ''));
  }
  sub irand64 {
    return irand() if ~0 == 4294967295;
    $_str .= _keystream(BUFSZ,$_state) if length($_str) < 8;
    ($a,$b) = unpack("V2",substr($_str, 0, 8, ''));
    return ($a << 32) | $b;
  }
  sub random_bytes {
    my($bytes) = @_;
    $bytes = (defined $bytes) ? int abs $bytes : 0;
    $_str .= _keystream($bytes-length($_str),$_state) if length($_str) < $bytes;
    return substr($_str, 0, $bytes, '');
  }
}

1;

__END__


# ABSTRACT:  Pure Perl ChaCha20 CSPRNG

=pod

=encoding utf8

=head1 NAME

Math::Prime::Util::ChaCha - Pure Perl ChaCha20 CSPRNG


=head1 VERSION

Version 0.73


=head1 SYNOPSIS

=head1 DESCRIPTION

A pure Perl implementation of ChaCha20 with a CSPRNG interface.

=head1 FUNCTIONS

=head2 csrand

Takes a binary string as input and seeds the internal CSPRNG.

=head2 srand

A method for sieving the CSPRNG with a small value.  This will not be secure
but can be useful for simulations and emulating the system C<srand>.

With no argument, chooses a random number, seeds and returns the number.
With a single integer argument, seeds and returns the number.

=head2 irand

Returns a random 32-bit integer.

=head2 irand64

Returns a random 64-bit integer.

=head2 random_bytes

Takes an unsigned number C<n> as input and returns that many random bytes
as a single binary string.

=head2

=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>

=head1 ACKNOWLEDGEMENTS

Daniel J. Bernstein wrote the ChaCha family of stream ciphers in 2008 as
an update to the popular Salsa20 cipher from 2005.

RFC7539: "ChaCha20 and Poly1305 for IETF Protocols" was used to create both
the C and Perl implementations.  Test vectors from that document are used
here as well.

For final optimizations I got ideas from Christopher Madsen's
L<Crypt::Salsa20> for how to best work around some of Perl's aggressive
dynamic typing.
Our core is still about 20% slower than Salsa20.

=head1 COPYRIGHT

Copyright 2017 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
