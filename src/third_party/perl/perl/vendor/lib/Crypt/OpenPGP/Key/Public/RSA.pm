package Crypt::OpenPGP::Key::Public::RSA;
use strict;

use Crypt::RSA::Key::Public;
use Crypt::OpenPGP::Digest;
use Crypt::OpenPGP::Util qw( bitsize bin2mp mp2bin );
use Crypt::OpenPGP::Key::Public;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::Key::Public Crypt::OpenPGP::ErrorHandler );

sub can_encrypt { 1 }
sub can_sign { 1 }
sub abbrev { 'R' }

sub public_props { qw( n e ) }
sub crypt_props { qw( c ) }
sub sig_props { qw( c ) }

sub init {
    my $key = shift;
    $key->{key_data} = shift || Crypt::RSA::Key::Public->new;
    $key;
}

sub keygen {
    my $class = shift;
    my %param = @_;
    $param{Password} = $param{Passphrase};
    require Crypt::RSA::Key;
    my $chain = Crypt::RSA::Key->new;
    my($pub, $sec) = $chain->generate( %param );
    return $class->error( $chain->errstr ) unless $pub && $sec;
    ($pub, $sec);
}

sub size { bitsize($_[0]->{key_data}->n) }

sub check { $_[0]->{key_data}->check }

sub encrypt {
    my $key = shift;
    my($M) = @_;
    require Crypt::RSA::Primitives;
    my $prim = Crypt::RSA::Primitives->new;
    my $c = $prim->core_encrypt( Key => $key->{key_data}, Plaintext => $M ) or
        return $key->error($prim->errstr);
    { c => $c }
}

sub verify {
    my $key = shift;
    my($sig, $dgst) = @_;
    my $k = $key->bytesize;
    require Crypt::RSA::Primitives;
    my $prim = Crypt::RSA::Primitives->new;
    my $c = $sig->{c};
    my $m = $prim->core_verify( Key => $key->{key_data}, Signature => $c) or
        return;
    $m = mp2bin($m, $k - 1);
    my $hash_alg = Crypt::OpenPGP::Digest->alg($sig->{hash_alg});
    my $M = encode($dgst, $hash_alg, $k - 1);
    $m eq $M;
}

{
    my %ENCODING = (
        MD2  => pack('H*', '3020300C06082A864886F70D020205000410'),
        MD5  => pack('H*', '3020300C06082A864886F70D020505000410'),
        SHA1 => pack('H*', '3021300906052B0E03021A05000414'),
    );

    sub encode {
        my($dgst, $hash_alg, $mlen) = @_;
        my $alg = $ENCODING{$hash_alg};
        my $m = $alg . $dgst;
        my $padlen = $mlen - length($m) - 2;
        my $pad = chr(255) x $padlen;
        chr(1) . $pad . chr(0) . $m;
    }
}

1;
