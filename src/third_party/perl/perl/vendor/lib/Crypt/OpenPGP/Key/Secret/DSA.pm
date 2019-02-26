package Crypt::OpenPGP::Key::Secret::DSA;
use strict;

use Crypt::DSA::Key;
use Crypt::OpenPGP::Key::Public::DSA;
use Crypt::OpenPGP::Key::Secret;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::Key::Secret Crypt::OpenPGP::ErrorHandler );

sub secret_props { qw( x ) }
*sig_props = \&Crypt::OpenPGP::Key::Public::DSA::sig_props;
*public_props = \&Crypt::OpenPGP::Key::Public::DSA::public_props;
*size = \&Crypt::OpenPGP::Key::Public::DSA::size;
*keygen = \&Crypt::OpenPGP::Key::Public::DSA::keygen;
*can_sign = \&Crypt::OpenPGP::Key::Public::DSA::can_sign;

sub init {
    my $key = shift;
    $key->{key_data} = shift || Crypt::DSA::Key->new;
    $key;
}

sub y { $_[0]->{key_data}->pub_key(@_[1..$#_]) }
sub x { $_[0]->{key_data}->priv_key(@_[1..$#_]) }

sub sign {
    my $key = shift;
    my($dgst) = @_;
    require Crypt::DSA;
    my $dsa = Crypt::DSA->new;
    my $sig = $dsa->sign(
                  Key => $key->{key_data},
                  Digest => $dgst,
              );
}

*verify = \&Crypt::OpenPGP::Key::Public::DSA::verify;

1;
