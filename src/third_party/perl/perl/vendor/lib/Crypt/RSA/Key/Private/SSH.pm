#!/usr/bin/perl -sw
##
## Crypt::RSA::Key::Private::SSH
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: SSH.pm,v 1.1 2001/05/20 23:37:47 vipul Exp $

package Crypt::RSA::Key::Private::SSH::Buffer;
use strict;
use Crypt::RSA::DataFormat qw( os2ip bitsize i2osp );
use Data::Buffer;
use base qw( Data::Buffer );

sub get_mp_int {
    my $buf = shift;
    my $off = $buf->{offset};
    my $bits = unpack "n", $buf->bytes($off, 2);
    my $bytes = int(($bits+7)/8);
    my $p = os2ip( $buf->bytes($off+2, $bytes) );
    $buf->{offset} += 2 + $bytes;
    $p;
}

sub put_mp_int {
    my $buf = shift;
    my $int = shift;
    my $bits = bitsize($int);
    $buf->put_int16($bits);
    $buf->put_chars( i2osp($int) );
}


package Crypt::RSA::Key::Private::SSH;
use FindBin qw($Bin);
use lib "$Bin/../../../../../lib";
use strict;
use constant PRIVKEY_ID => "SSH PRIVATE KEY FILE FORMAT 1.1\n";
use vars qw( %CIPHERS );

BEGIN {
    %CIPHERS = (
        1 => 'IDEA',
        2 => 'DES',
        3 => 'DES3',
        4 => 'ARCFOUR',
        6 => 'Blowfish',
    );
}

use Carp qw( croak );
use Data::Buffer;
use Crypt::CBC;
use Crypt::RSA::Key::Private;
use base qw( Crypt::RSA::Key::Private );

sub deserialize {
    my($key, %params) = @_;
    my $blob = join '', @{$params{String}};
    my $passphrase = $params{Passphrase} || '';

    my $buffer = new Crypt::RSA::Key::Private::SSH::Buffer;
    $buffer->append($blob);

    my $id = $buffer->bytes(0, length(PRIVKEY_ID), '');
    croak "Bad key file format" unless $id eq PRIVKEY_ID;
    $buffer->bytes(0, 1, '');

    my $cipher_type = $buffer->get_int8;
    $buffer->get_int32;   ## Reserved data.

    $buffer->get_int32;   ## Private key bits.
    $key->n( $buffer->get_mp_int );
    $key->e( $buffer->get_mp_int );

    $key->Identity( $buffer->get_str );     ## Comment.

    if ($cipher_type != 0) {
        my $cipher_name = $CIPHERS{$cipher_type} or
            croak "Unknown cipher '$cipher_type' used in key file";
        my $class = 'Crypt::' . $cipher_name;
        eval { require $class };
        if ($@) { croak "Unsupported cipher '$cipher_name': $@" }

        my $cipher = Crypt::CBC->new($passphrase, $cipher_name);
        my $decrypted =
            $cipher->decrypt($buffer->bytes($buffer->offset));
        $buffer->empty;
        $buffer->append($decrypted);
    }

    my $check1 = $buffer->get_int8;
    my $check2 = $buffer->get_int8;
    unless ($check1 == $buffer->get_int8 &&
            $check2 == $buffer->get_int8) {
        croak "Bad passphrase supplied for key file";
    }

    $key->d( $buffer->get_mp_int );
    $key->u( $buffer->get_mp_int );
    $key->p( $buffer->get_mp_int );
    $key->q( $buffer->get_mp_int );

    $key;
}


sub serialize {
    my($key, %params) = @_;
    my $passphrase = $params{Password} || '';
    my $cipher_type = $passphrase eq '' ? 0 :
        $params{Cipher} || 3;
 
    my $buffer = new Crypt::RSA::Key::Private::SSH::Buffer;
    my($check1, $check2);
    $buffer->put_int8($check1 = int rand 255);
    $buffer->put_int8($check2 = int rand 255);
    $buffer->put_int8($check1);
    $buffer->put_int8($check2);

    $buffer->put_mp_int($key->d);
    $buffer->put_mp_int($key->u);
    $buffer->put_mp_int($key->p);
    $buffer->put_mp_int($key->q);

    $buffer->put_int8(0)
        while $buffer->length % 8;

    my $encrypted = new Crypt::RSA::Key::Private::SSH::Buffer;
    $encrypted->put_chars(PRIVKEY_ID);
    $encrypted->put_int8(0);
    $encrypted->put_int8($cipher_type);
    $encrypted->put_int32(0);

    $encrypted->put_int32(Crypt::RSA::DataFormat::bitsize($key->n));
    $encrypted->put_mp_int($key->n);
    $encrypted->put_mp_int($key->e);
    $encrypted->put_str($key->Identity || '');

    if ($cipher_type) {
        my $cipher_name = $CIPHERS{$cipher_type};
        my $class = 'Crypt::' . $cipher_name;
        eval { require $class };
        if ($@) { croak "Unsupported cipher '$cipher_name': $@" }
    
        my $cipher = Crypt::CBC->new($passphrase, $cipher_name);
        $encrypted->append( $cipher->encrypt($buffer->bytes) );
    }
    else {
        $encrypted->append($buffer->bytes);
    }
    
    $encrypted->bytes;
}


sub hide {} 

=head1 NAME

Crypt::RSA::Key::Private::SSH - SSH Private Key Import

=head1 SYNOPSIS

    Crypt::RSA::Key::Private::SSH is a class derived from
    Crypt::RSA::Key::Private that provides serialize() and
    deserialze() methods for SSH 2.x keys.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=cut




1;
