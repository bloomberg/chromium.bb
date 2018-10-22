package Crypt::DSA::Util;

use strict;
use Math::BigInt 1.78 try => 'GMP, Pari';
use Fcntl;
use Carp qw( croak );

use vars qw( $VERSION @ISA @EXPORT_OK );
use Exporter;
BEGIN {
    $VERSION   = '1.17';
    @ISA       = qw( Exporter );
    @EXPORT_OK = qw( bitsize bin2mp mp2bin mod_inverse mod_exp makerandom isprime );
}

## Nicked from Crypt::RSA::DataFormat.
## Copyright (c) 2001, Vipul Ved Prakash.
sub bitsize {
    length(Math::BigInt->new($_[0])->as_bin) - 2;
}

sub bin2mp {
    my $s = shift;
    $s eq '' ?
        Math::BigInt->new(0) :
        Math::BigInt->new("0b" . unpack("B*", $s));
}

sub mp2bin {
    my $p = Math::BigInt->new(shift);
    my $base = Math::BigInt->new(256);
    my $res = '';
    while ($p != 0) {
        my $r = $p % $base;
        $p = ($p-$r) / $base;
        $res = chr($r) . $res;
    }
    $res;
}

sub mod_exp {
    my($a, $exp, $n) = @_;
    $a->copy->bmodpow($exp, $n);
}

sub mod_inverse {
    my($a, $n) = @_;
    $a->copy->bmodinv($n);
}

sub makerandom {
    my %param = @_;
    my $size = $param{Size};
    my $bytes = int($size / 8) + 1;
    my $r = '';
    if ( sysopen my $fh, '/dev/random', O_RDONLY ) {
        my $read = 0;
        while ($read < $bytes) {
            my $got = sysread $fh, my($chunk), $bytes - $read;
            next unless $got;
            die "Error: $!" if $got == -1;
            $r .= $chunk;
            $read = length $r;
        }
        close $fh;
    }
    elsif ( require Data::Random ) {
        $r .= Data::Random::rand_chars( set=>'numeric' ) for 1..$bytes;
    }
    else {
        croak "makerandom requires /dev/random or Data::Random";
    }
    my $down = $size - 1;
    $r = unpack 'H*', pack 'B*', '0' x ( $size % 8 ? 8 - $size % 8 : 0 ) .
        '1' . unpack "b$down", $r;
    Math::BigInt->new('0x' . $r);
}

# For testing, let us choose our isprime function:
*isprime = \&isprime_algorithms_with_perl;

# from the book "Mastering Algorithms with Perl" by Jon Orwant,
# Jarkko Hietaniemi, and John Macdonald
sub isprime_algorithms_with_perl {
    use integer;
    my $n = shift;
    my $n1 = $n - 1;
    my $one = $n - $n1;  # not just 1, but a bigint
    my $witness = $one * 100;

    # find the power of two for the top bit of $n1
    my $p2 = $one;
    my $p2index = -1;
    ++$p2index, $p2 *= 2
	while $p2 <= $n1;
    $p2 /= 2;

    # number of interations:  5 for 260-bit numbers, go up to 25 for smaller
    my $last_witness = 5;
    $last_witness += (260 - $p2index) / 13 if $p2index < 260;

    for my $witness_count (1..$last_witness) {
	$witness *= 1024;
	$witness += int(rand(1024));  # XXXX use good rand
	$witness = $witness % $n if $witness > $n;
	$witness = $one * 100, redo if $witness == 0;

	my $prod = $one;
	my $n1bits = $n1;
	my $p2next = $p2;

	# compute $witness ** ($n - 1)
	while (1) {
	    my $rootone = $prod == 1 || $prod == $n1;
	    $prod = ($prod * $prod) % $n;
	    return 0 if $prod == 1 && ! $rootone;
	    if ($n1bits >= $p2next) {
		$prod = ($prod * $witness) % $n;
		$n1bits -= $p2next;
	    }
	    last if $p2next == 1;
	    $p2next /= 2;
	}
	return 0 unless $prod == 1;
    }
    return 1;
}

sub isprime_gp_pari {
    my $n = shift;

    my $sn = "$n";
    die if $sn =~ /\D/;

    my $is_prime = `echo "isprime($sn)" | gp -f -q`;
    die "No gp installed?" if $?;

    chomp $is_prime;
    return $is_prime;
}

sub isprime_paranoid {
    my $n = shift;

    my $perl = isprime_algorithms_with_perl($n);
    my $pari = isprime_gp_pari($n);

    die "Perl vs. PARI don't match on '$n'\n" unless $perl == $pari;
    return $perl;
}

1;
__END__

=head1 NAME

Crypt::DSA::Util - DSA Utility functions

=head1 SYNOPSIS

    use Crypt::DSA::Util qw( func1 func2 ... );

=head1 DESCRIPTION

I<Crypt::DSA::Util> contains a set of exportable utility functions
used through the I<Crypt::DSA> set of libraries.

=head2 bitsize($n)

Returns the number of bits in the I<Math::Pari> integer object
I<$n>.

=head2 bin2mp($string)

Given a string I<$string> of any length, treats the string as a
base-256 representation of an integer, and returns that integer,
a I<Math::Pari> object.

=head2 mp2bin($int)

Given a biginteger I<$int> (a I<Math::Pari> object), linearizes
the integer into an octet string, and returns the octet string.

=head2 mod_exp($a, $exp, $n)

Computes $a ^ $exp mod $n and returns the value. The calculations
are done using I<Math::Pari>, and the return value is a I<Math::Pari>
object.

=head2 mod_inverse($a, $n)

Computes the multiplicative inverse of $a mod $n and returns the
value. The calculations are done using I<Math::Pari>, and the
return value is a I<Math::Pari> object.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::DSA manpage for author, copyright,
and license information.

=cut
