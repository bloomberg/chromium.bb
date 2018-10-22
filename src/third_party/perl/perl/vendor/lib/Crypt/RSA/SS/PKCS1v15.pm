#!/usr/bin/perl -sw
##
## Crypt::RSA::SS:PKCS1v15
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: PKCS1v15.pm,v 1.6 2001/06/22 23:27:38 vipul Exp $

package Crypt::RSA::SS::PKCS1v15;
use strict;
use base 'Crypt::RSA::Errorhandler';
use Crypt::RSA::DataFormat qw(octet_len os2ip i2osp h2osp);
use Crypt::RSA::Primitives;
use Crypt::RSA::Debug qw(debug);
use Digest::SHA1 qw(sha1);
use Digest::MD5 qw(md5);
use Digest::MD2 qw(md2);
use Math::Pari qw(floor);

$Crypt::RSA::SS::PKCS1v15::VERSION = '1.99';

sub new { 

    my ($class, %params) = @_;
    my $self = bless { 
                       primitives => new Crypt::RSA::Primitives, 
                       digest     => $params{Digest} || 'SHA1',
                       encoding   => { 
                                        MD2 => "0x 30 20 30 0C 06 08 2A 86 48
                                                   86 F7 0D 02 02 05 00 04 10",
                                        MD5 => "0x 30 20 30 0C 06 08 2A 86 48
                                                   86 F7 0D 02 05 05 00 04 10",
                                       SHA1 => "0x 30 21 30 09 06 05 2B 0E 03
                                                   02 1A 05 00 04 14",
                                     },
                       VERSION    => $Crypt::RSA::SS::PKCS1v15::VERSION,
                     }, $class;           
    if ($params{Version}) { 
        # do versioning here
    }
    return $self;

}


sub sign { 

    my ($self, %params) = @_; 
    my $key = $params{Key}; my $M = $params{Message} || $params{Plaintext};
    return $self->error ("No Message or Plaintext parameter", \$key, \%params) unless $M;
    return $self->error ("No Key parameter", \$M, \%params) unless $key;
    my $k = octet_len ($key->n);

    my $em; 
    unless ($em = $self->encode ($M, $k-1)) { 
        return $self->error ($self->errstr, \$key, \%params, \$M) 
            if $self->errstr eq "Message too long.";
        return $self->error ("Modulus too short.", \$key, \%params, \$M)
            if $self->errstr eq "Intended encoded message length too short";
    }

    my $m = os2ip ($em);
    my $sig = $self->{primitives}->core_sign (Key => $key, Message => $m);
    return i2osp ($sig, $k);

}    


sub verify { 

    my ($self, %params) = @_;
    my $key = $params{Key}; my $M = $params{Message} || $params{Plaintext};
    my $S = $params{Signature}; 
    return $self->error ("No Message or Plaintext parameter", \$key, \%params) unless $M;
    return $self->error ("No Key parameter", \$M, \$S, \%params) unless $key;
    return $self->error ("No Signature parameter", \$key, \$M, \%params) unless $S;
    my $k = octet_len ($key->n);
    return $self->error ("Invalid signature.", \$key, \$M, \%params) if length($S) != $k;
    my $s = os2ip ($S);
    my $m = $self->{primitives}->core_verify (Key => $key, Signature => $s) || 
        $self->error ("Invalid signature.", \$M, $key, \%params);
    my $em = i2osp ($m, $k-1) || 
        return $self->error ("Invalid signature.", \$M, \$S, $key, \%params);
    my $em1; 
    unless ($em1 = $self->encode ($M, $k-1)) { 
        return $self->error ($self->errstr, \$key, \%params, \$M) 
            if $self->errstr eq "Message too long.";
        return $self->error ("Modulus too short.", \$key, \%params, \$M)
            if $self->errstr eq "Intended encoded message length too short.";
    }

    debug ("em: $em"); debug ("em1: $em1");

    return 1 if $em eq $em1;
    return $self->error ("Invalid signature.", \$M, \$key, \%params);

}


sub encode { 

    my ($self, $M, $emlen) = @_;

    my $H;
    if    ($self->{digest} eq "SHA1") { $H = sha1 ($M) }
    elsif ($self->{digest} eq "MD5" ) { $H =  md5 ($M) }
    elsif ($self->{digest} eq "MD2" ) { $H =  md2 ($M) }

    my $alg = h2osp($self->{encoding}->{$self->{digest}});
    my $T = $alg . $H;
    $self->error ("Intended encoded message length too short.", \$M) if $emlen < length($T) + 10;
    my $pslen = $emlen - length($T) - 2;
    my $PS = chr(0xff) x $pslen;
    my $em = chr(1) . $PS . chr(0) . $T; 
    return $em;

}


sub version { 
    my $self = shift;
    return $self->{VERSION};
}


sub signblock { 
    return -1;
}


sub verifyblock { 
    my ($self, %params) = @_;
    return octet_len($params{Key}->n);
}


1;

=head1 NAME

Crypt::RSA::SS::PKCS1v15 - PKCS #1 v1.5 signatures. 

=head1 SYNOPSIS

    my $pkcs = new Crypt::RSA::SS::PKCS1v15 ( 
                        Digest => 'MD5'
                    );

    my $signature = $pkcs->sign (
                        Message => $message,
                        Key     => $private, 
                    ) || die $pss->errstr;

    my $verify    = $pkcs->verify (
                        Message   => $message, 
                        Key       => $key, 
                        Signature => $signature, 
                    ) || die $pss->errstr;


=head1 DESCRIPTION

This module implements PKCS #1 v1.5 signatures based on RSA. See [13] 
for details on the scheme.

=head1 METHODS

=head2 B<new()>

Constructor.   Takes a hash as argument with the following key: 

=over 4

=item B<Digest> 

Name of the Message Digest algorithm. Three Digest algorithms are
supported: MD2, MD5 and SHA1. Digest defaults to SHA1.

=back 


=head2 B<version()>

Returns the version number of the module. 

=head2 B<sign()>

Computes a PKCS #1 v1.5 signature on a message with the private key of the
signer. sign() takes a hash argument with the following mandatory keys:

=over 4

=item B<Message> 

Message to be signed, a string of arbitrary length.  

=item B<Key> 

Private key of the signer, a Crypt::RSA::Key::Private object.

=back

=head2 B<verify()>

Verifies a signature generated with sign(). Returns a true value on
success and false on failure. $self->errstr is set to "Invalid signature."
or appropriate error on failure. verify() takes a hash argument with the
following mandatory keys:

=over 4

=item B<Key>

Public key of the signer, a Crypt::RSA::Key::Public object. 

=item B<Message>

The original signed message, a string of arbitrary length. 

=item B<Signature>

Signature computed with sign(), a string. 

=back

=head1 ERROR HANDLING

See ERROR HANDLING in Crypt::RSA(3) manpage.

=head1 BIBLIOGRAPHY

See BIBLIOGRAPHY in Crypt::RSA(3) manpage.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 SEE ALSO 

Crypt::RSA(3), Crypt::RSA::Primitives(3), Crypt::RSA::Keys(3),
Crypt::RSA::EME::OAEP(3)

=cut


