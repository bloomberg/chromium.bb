package Crypt::OpenPGP::Key::Public::ElGamal;
use strict;

use Crypt::OpenPGP::Util qw( bitsize);
use Crypt::OpenPGP::Key::Public;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::Key::Public Crypt::OpenPGP::ErrorHandler );

sub can_encrypt { 1 }
sub abbrev { 'g' }

sub public_props { qw( p g y ) }
sub crypt_props { qw( a b ) }
sub sig_props { qw( a b ) }

sub size { bitsize($_[0]->p) }

sub init {
    my $key = shift;
    $key->{key_data} = shift || Crypt::OpenPGP::ElGamal::Public->new;
    $key;
}

sub keygen {
    return $_[0]->error("ElGamal key generation is not supported");
}

sub encrypt {
    my $key = shift;
    my($M) = @_;
    $key->{key_data}->encrypt($M);
}

package Crypt::OpenPGP::ElGamal::Public;
use strict;

use Crypt::OpenPGP::Util qw( mod_exp );
use Math::BigInt;

sub new { bless {}, $_[0] }

sub encrypt {
    my $key = shift;
    my($M) = @_;
    my $k = gen_k($key->p);
    my $a = mod_exp($key->g, $k, $key->p);
    my $b = mod_exp($key->y, $k, $key->p);
    $b->bmod($key->p);  
    { a => $a, b => $b * $M };
}

sub gen_k {
    my($p) = @_;
    ## XXX choose bitsize based on bitsize of $p
    my $bits = 198;
    my $p_minus1 = $p - 1;

    my $k = Crypt::OpenPGP::Util::get_random_bigint($bits);
    while (1) {
        last if Math::BigInt::bgcd($k, $p_minus1) == 1;
        $k++;
    }
    $k;
}

sub _getset {
    my $e = shift;
    sub {
        my $key = shift;
        $key->{$e} = shift if @_;
        $key->{$e};
    }
}

*p = _getset('p');
*g = _getset('g');
*y = _getset('y');

1;
