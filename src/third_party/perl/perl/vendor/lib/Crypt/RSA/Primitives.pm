#!/usr/bin/perl -sw
##
## Crypt::RSA::Primitives -- Cryptography and encoding primitives  
##                           used by Crypt::RSA.
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Primitives.pm,v 1.14 2001/06/22 23:27:35 vipul Exp $

package Crypt::RSA::Primitives; 
use strict;
use base 'Crypt::RSA::Errorhandler';
use Crypt::RSA::Debug qw(debug);
use Math::Pari qw(PARI Mod lift);
use Carp;

sub new { 
    return bless {}, shift; 
} 


sub core_encrypt {

    # procedure: 
    # c = (m ** e) mod n 

    my ($self, %params) = @_;
    my $key = $params{Key}; 
    $self->error ("Bad key.", \%params, $key) unless $key->check();
    my $plaintext = $params{Message} || $params{Plaintext};
    debug ("pt == $plaintext");

    my $e = $key->e; my $n = $key->n;
    return $self->error ("Numeric representation of plaintext is out of bound.", 
                          \$plaintext, $key, \%params) if $plaintext > $n;
    my $c = mod_exp($plaintext, $e, $n);
    debug ("ct == $c");
    $n = undef; $e = undef;
    return $c;

}


sub core_decrypt {

    # procedure: 
    # p = (c ** d) mod n


    my ($self, %params) = @_;
    my $key = $params{Key}; 
    $self->error ("Bad key.") unless $key->check();

    my $cyphertext = $params{Cyphertext} || $params{Ciphertext};
    my $n = $key->n;
    return $self->error ("Decryption error.") if $cyphertext > $n;

    my $pt;
    if ($key->p && $key->q) {
        my($p, $q, $d) = ($key->p, $key->q, $key->d);
        $key->u (mod_inverse($p, $q)) unless $key->u;
        $key->dp ($d % ($p-1)) unless $key->dp;
        $key->dq ($d % ($q-1)) unless $key->dq;
        my $p2 = mod_exp($cyphertext % $p, $key->dp, $p);
        my $q2 = mod_exp($cyphertext % $q, $key->dq, $q);
        $pt = $p2 + ($p * ((($q2 - $p2) * $key->u) % $q));
    }
    else {
        $pt = mod_exp ($cyphertext, $key->d, $n);
    }

    debug ("ct == $cyphertext");
    debug ("pt == $pt");
    return $pt;

}


sub core_sign { 

    my ($self, %params) = @_; 
    $params{Cyphertext} = $params{Message} || $params{Plaintext};
    return $self->core_decrypt (%params); 

} 


sub core_verify { 

    my ($self, %params) = @_; 
    $params{Plaintext} = $params{Signature};
    return $self->core_encrypt (%params); 

}


sub mod_inverse {
    my($a, $n) = @_;
    my $m = Mod(1, $n);
    lift($m / $a);
}


sub mod_exp {
    my($a, $exp, $n) = @_;
    my $m = Mod($a, $n);
    lift($m ** $exp);
}


1;

=head1 NAME

Crypt::RSA::Primitives - RSA encryption, decryption, signature and verification primitives. 

=head1 SYNOPSIS

    my $prim = new Crypt::RSA::Primitives;
    my $ctxt = $prim->core_encrypt (Key => $key, Plaintext => $string); 
    my $ptxt = $prim->core_decrypt (Key => $key, Cyphertext => $ctxt);
    my $sign = $prim->core_sign    (Key => $key, Message => $string); 
    my $vrfy = $prim->core_verify  (Key => $key, Signature => $sig);

=head1 DESCRIPTION

This module implements RSA encryption, decryption, signature and
verfication primitives. These primitives should only be used in the
context of an encryption or signing scheme. See Crypt::RSA::ES::OAEP(3),
and Crypt::RSA::SS::PSS(3) for the implementation of two such schemes.

=head1 ERROR HANDLING

See B<ERROR HANDLING> in Crypt::RSA(3) manpage.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 SEE ALSO 

Crypt::RSA(3), Crypt::RSA::Key(3), Crypt::RSA::ES::OAEP(3), 
Crypt::RSA::SS::PSS(3)

=cut


