package Crypt::OpenPGP::Util;
use strict;

# For some reason, FastCalc causes problems. Restrict to one of these 3 backends
use Math::BigInt only => 'Pari,GMP,Calc';

use vars qw( @EXPORT_OK @ISA );
use Exporter;
@EXPORT_OK = qw( bitsize bin2bigint bin2mp bigint2bin mp2bin mod_exp mod_inverse
                 dash_escape dash_unescape canonical_text );
@ISA = qw( Exporter );

sub bitsize {
    my $bigint = Math::BigInt->new($_[0]);
    return $bigint->bfloor($bigint->blog(2)) + 1;   
}

sub bin2bigint { $_[0] ? Math::BigInt->new('0x' . unpack 'H*', $_[0]) : 0 }

*bin2mp = \&bin2bigint;

sub bigint2bin {
    my($p) = @_;
        
    $p = _ensure_bigint($p);
    
    my $base = _ensure_bigint(1) << _ensure_bigint(4*8);
    my $res = '';
    while ($p != 0) {
        my $r = $p % $base;
        $p = ($p-$r) / $base;
        my $buf = pack 'N', $r;
        if ($p == 0) {
            $buf = $r >= 16777216 ? $buf :
                   $r >= 65536 ? substr($buf, -3, 3) :
                   $r >= 256   ? substr($buf, -2, 2) :
                                 substr($buf, -1, 1);
        } 
        $res = $buf . $res;
    }
    $res;
}

*mp2bin = \&bigint2bin;

sub mod_exp {
    my($a, $exp, $n) = @_;
    
    $a = _ensure_bigint($a);
    
    $a->copy->bmodpow($exp, $n);
}

sub mod_inverse {
    my($a, $n) = @_;
    
    $a = _ensure_bigint($a);

    $a->copy->bmodinv($n);
}

sub dash_escape {
    my($data) = @_;
    $data =~ s/^-/- -/mg;
    $data;
}

sub dash_unescape {
    my($data) = @_;
    $data =~ s/^-\s//mg;
    $data;
}

sub canonical_text {
    my($text) = @_;
    my @lines = split /\n/, $text, -1;
    for my $l (@lines) {
## pgp2 and pgp5 do not trim trailing whitespace from "canonical text"
## signatures, only from cleartext signatures.
## See:
##   http://cert.uni-stuttgart.de/archive/ietf-openpgp/2000/01/msg00033.html
        if ($Crypt::OpenPGP::Globals::Trim_trailing_ws) {
            $l =~ s/[ \t\r\n]*$//;
        } else {
            $l =~ s/[\r\n]*$//;
        }
    }
    join "\r\n", @lines;
}


sub _ensure_bigint {
    my $num = shift;

    if ($num && (! ref $num || ! $num->isa('Math::BigInt'))) {
        $num = Math::BigInt->new($num);
    }
    
    return $num;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Util - Miscellaneous utility functions

=head1 DESCRIPTION

I<Crypt::OpenPGP::Util> contains a set of exportable utility functions
used through the I<Crypt::OpenPGP> set of libraries.

=head2 bitsize($n)

Returns the number of bits in the I<Math::BigInt> integer object
I<$n>.

=head2 bin2bigint($string)

Given a string I<$string> of any length, treats the string as a
base-256 representation of an integer, and returns that integer,
a I<Math::BigInt> object.

I<bin2mp> is an alias for this function, for backwards
compatibility reasons.

=head2 bigint2bin($int)

Given a biginteger I<$int> (a I<Math::BigInt> object), linearizes
the integer into an octet string, and returns the octet string.

I<mp2bin> is an alias for this function, for backwards
compatibility reasons.

=head2 mod_exp($a, $exp, $n)

Computes $a ^ $exp mod $n and returns the value. The calculations
are done using I<Math::BigInt>, and the return value is a I<Math::BigInt>
object.

=head2 mod_inverse($a, $n)

Computes the multiplicative inverse of $a mod $n and returns the
value. The calculations are done using I<Math::BigInt>, and the
return value is a I<Math::BigInt> object.

=head2 canonical_text($text)

Takes a piece of text content I<$text> and formats it into PGP canonical
text, where: 1) all whitespace at the end of lines is stripped, and
2) all line endings are made up of a carriage return followed by a line
feed. Returns the canonical form of the text.

=head2 dash_escape($text)

Escapes I<$text> for use in a cleartext signature; the escaping looks
for any line starting with a dash, and on such lines prepends a dash
('-') followed by a space (' '). Returns the escaped text.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
