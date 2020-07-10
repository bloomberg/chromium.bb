package Crypt::RSA::ES::PKCS1v15;
use strict;
use warnings;

## Crypt::RSA::ES::PKCS1v15
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.

use base 'Crypt::RSA::Errorhandler';
use Math::Prime::Util qw/random_bytes/;
use Crypt::RSA::DataFormat qw(bitsize octet_len os2ip i2osp);
use Crypt::RSA::Primitives;
use Crypt::RSA::Debug      qw(debug);
use Carp;

$Crypt::RSA::ES::PKCS1v15::VERSION = '1.99';

sub new { 
    my ($class, %params) = @_;
    my $self = bless { primitives => new Crypt::RSA::Primitives, 
                       VERSION    => $Crypt::RSA::ES::PKCS1v15::VERSION,
                      }, $class;
    if ($params{Version}) { 
        # do versioning here.
    }
    return $self;
}


sub encrypt { 
    my ($self, %params) = @_; 
    my $key = $params{Key}; my $M = $params{Message} || $params{Plaintext};
    return $self->error ("No Message or Plaintext parameter", \$key, \%params) unless $M;
    return $self->error ($key->errstr, \$M, $key, \%params) unless $key->check;
    my $k = octet_len ($key->n);  debug ("octet_len of modulus: $k");
    my $em = $self->encode ($M, $k-1) || 
        return $self->error ($self->errstr, \$M, $key, \%params);
        debug ("encoded: $em");
    my $m = os2ip ($em);
    my $c = $self->{primitives}->core_encrypt (Plaintext => $m, Key => $key);
    my $ec = i2osp ($c, $k);  debug ("cyphertext: $ec");
    return $ec;
}    


sub decrypt { 
    my ($self, %params) = @_;
    my $key = $params{Key}; my $C = $params{Cyphertext} || $params{Ciphertext};
    return $self->error ("No Cyphertext or Ciphertext parameter", \$key, \%params) unless $C;
    return $self->error ($key->errstr, $key, \%params) unless $key->check;
    my $k = octet_len ($key->n);
    my $c = os2ip ($C);
    debug ("bitsize(c): " . bitsize($c));
    debug ("bitsize(n): " . bitsize($key->n));
    if (bitsize($c) > bitsize($key->n)) { 
        return $self->error ("Decryption error.", $key, \%params) 
    }
    my $m = $self->{primitives}->core_decrypt (Cyphertext => $c, Key => $key) || 
        return $self->error ("Decryption error.", $key, \%params);
    my $em = i2osp ($m, $k-1) || 
        return $self->error ("Decryption error.", $key, \%params);
    my $M; $self->errstrrst;  # reset the errstr
    unless ($M = $self->decode ($em)) { 
        return $self->error ("Decryption error.", $key, \%params) if $self->errstr();
        return $M;
    } 
    return $M;
} 


sub encode { 
    my ($self, $M, $emlen) = @_; 
    $M = $M || ""; my $mlen = length($M);
    return $self->error ("Message too long.", \$M) if $mlen > $emlen-10;

    my $pslen = $emlen-$mlen-2;
    # my $PS = join('', map { chr( 1+urandomm(255) ) } 1 .. $pslen);
    my $PS = '';
    while (length($PS) < $pslen) {
      $PS .= random_bytes( $pslen - length($PS) );
      $PS =~ s/\x00//g;
    }
    my $em = chr(2).$PS.chr(0).$M;
    return $em;
}


sub decode { 
    my ($self, $em) = @_; 

    return $self->error ("Decoding error.") if length($em) < 10;

    debug ("to decode: $em");
    my ($chr0, $chr2) = (chr(0), chr(2));
    my ($ps, $M);
    unless ( ($ps, $M) = $em =~ /^$chr2(.*?)$chr0(.*)$/s ) { 
        return $self->error ("Decoding error.");
    }
    return $self->error ("Decoding error.") if length($ps) < 8; 
    return $M;
}


sub encryptblock { 
    my ($self, %params) = @_;
    return octet_len ($params{Key}->n) - 11;
} 


sub decryptblock { 
    my ($self, %params) = @_;
    return octet_len ($params{Key}->n);
}


sub version {
    my $self = shift;
    return $self->{VERSION};
}


1;

=head1 NAME

Crypt::RSA::ES::PKCS1v15 - PKCS #1 v1.5 padded encryption scheme based on RSA.

=head1 SYNOPSIS

    my $pkcs = new Crypt::RSA::ES::PKCS1v15; 

    my $ct = $pkcs->encrypt( Key => $key, Message => $message ) || 
                die $pkcs->errstr; 

    my $pt = $pkcs->decrypt( Key => $key, Cyphertext => $ct )   || 
                die $pkcs->errstr; 

=head1 DESCRIPTION

This module implements PKCS #1 v1.5 padded encryption scheme based on RSA.
See [13] for details on the encryption scheme.

=head1 METHODS

=head2 B<new()>

Constructor. 

=head2 B<version()>

Returns the version number of the module.

=head2 B<encrypt()>

Encrypts a string with a public key and returns the encrypted string
on success. encrypt() takes a hash argument with the following
mandatory keys:

=over 4

=item B<Message>

A string to be encrypted. The length of this string should not exceed k-10
octets, where k is the octet length of the RSA modulus. If Message is
longer than k-10, the method will fail and set $self->errstr to "Message
too long."

=item B<Key>

Public key of the recipient, a Crypt::RSA::Key::Public object.

=back

=head2 B<decrypt()>

Decrypts cyphertext with a private key and returns plaintext on
success. $self->errstr is set to "Decryption Error." or appropriate
error on failure. decrypt() takes a hash argument with the following
mandatory keys:

=over 4

=item B<Cyphertext>

A string encrypted with encrypt(). The length of the cyphertext must be k
octets, where k is the length of the RSA modulus.

=item B<Key>

Private key of the receiver, a Crypt::RSA::Key::Private object.

=back

=head1 ERROR HANDLING

See ERROR HANDLING in Crypt::RSA(3) manpage.

=head1 BIBLIOGRAPHY 

See BIBLIOGRAPHY in Crypt::RSA(3) manpage.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 SEE ALSO 

Crypt::RSA(3), Crypt::RSA::Primitives(3), Crypt::RSA::Keys(3),
Crypt::RSA::SSA::PSS(3)

=cut


