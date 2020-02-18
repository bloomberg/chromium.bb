package Crypt::RSA::Key::Private::SSH::Buffer;
use strict;
use warnings;

## Crypt::RSA::Key::Private::SSH
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.

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
use strict;
use warnings;
use constant PRIVKEY_ID => "SSH PRIVATE KEY FILE FORMAT 1.1\n";
use vars qw( %CIPHERS %CIPHERS_TEXT );

# Having to name all the ciphers here is not extensible, but we're stuck
# with it given the RSA1 format.  I don't think any of this is standardized.
# OpenSSH supports only: none, des, 3des, and blowfish here.  This set of
# numbers below 10 match.  Values above 10 are well supported by Perl modules.
BEGIN {
    # CIPHERS : Used by deserialize to map numbers to modules.
    %CIPHERS = (
       # 0 = none
        1 => [ 'IDEA' ],
        2 => [ 'DES', 'DES_PP' ],
        3 => [ 'DES_EDE3' ],
# From what I can see, none of the 3+ RC4 modules are CBC compatible
#        5 => [ 'RC4' ],
        6 => [ 'Blowfish', 'Blowfish_PP' ],
       10 => [ 'Twofish2' ],
       11 => [ 'CAST5', 'CAST5_PP' ],
       12 => [ 'Rijndael', 'OpenSSL::AES' ],
       13 => [ 'RC6' ],
       14 => [ 'Camellia', 'Camellia_PP' ],
# Crypt::Serpent is broken and abandonded.
    );
    # CIPHERS_TEXT : Used by serialize to map names to modules to numbers
    %CIPHERS_TEXT = (
      'NONE'       => 0,
      'IDEA'       => 1,
      'DES'        => 2,
      'DES_EDE3'   => 3,
      'DES3'       => 3,
      '3DES'       => 3,
      'TRIPLEDES'  => 3,
#      'RC4'        => 5,
#      'ARC4'       => 5,
#      'ARCFOUR'    => 5,
      'BLOWFISH'   => 6,
      'TWOFISH'    => 10,
      'TWOFISH2'   => 10,
      'CAST5'      => 11,
      'CAST5_PP'   => 11,
      'CAST5PP'    => 11,
      'CAST-5'     => 11,
      'CAST-128'   => 11,
      'CAST128'    => 11,
      'RIJNDAEL'   => 12,
      'AES'        => 12,
      'OPENSSL::AES'=>12,
      'RC6'        => 13,
      'CAMELLIA'   => 14,
    );
}

use Carp qw( croak );
use Data::Buffer;
use Crypt::CBC 2.17;   # We want a good version
use Crypt::RSA::Key::Private;
use base qw( Crypt::RSA::Key::Private );

sub deserialize {
    my($key, %params) = @_;
    my $passphrase = defined $params{Password} ? $params{Password}
                   : defined $key->Password    ? $key->Password
                   : '';
    my $string = $params{String};
    croak "Must supply String=>'blob' to deserialize" unless defined $string;
    $string = join('', @$string) if ref($string) eq 'ARRAY';

    croak "Cowardly refusing to deserialize on top of a hidden key"
      if $key->{Hidden};

    my $buffer = new Crypt::RSA::Key::Private::SSH::Buffer;
    $buffer->append($string);

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
        my $cipher_names = $CIPHERS{$cipher_type} or
            croak "Unknown cipher '$cipher_type' used in key file";
        my $cipher_name;
        foreach my $name (@$cipher_names) {
          my $class = "Crypt::$name";
          (my $file = $class) =~ s=::|'=/=g;
          if ( eval { require "$file.pm"; 1 } ) {
            $cipher_name = $name; last;
          }
        }
        if (!defined $cipher_name) {
          croak "Unsupported cipher '$cipher_names->[0]': $@";
        }

        my $cipher = Crypt::CBC->new( -key    => $passphrase,
                                      -cipher => $cipher_name );
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

    # Restore other variables.
    $key->phi( ($key->p - 1) * ($key->q - 1) );
    $key->dp( $key->d % ($key->p - 1) );
    $key->dq( $key->d % ($key->q - 1) );
    # Our passphrase may be just temporary for the serialization, and have
    # nothing to do with the key.  So don't store it.
    #$key->{Password} = $passphrase unless defined $key->{Password};

    $key;
}


sub serialize {
    my($key, %params) = @_;

    # We could reveal it, but (1) what if it was hidden with a different
    # password, and (2) they may not want to revealed (even if hidden after).
    croak "Cowardly refusing to serialize a hidden key"
      if $key->{Hidden};

    my $passphrase = defined $params{Password} ? $params{Password}
                   : defined $key->Password    ? $key->Password
                   : '';
    my $cipher_name = defined $params{Cipher} ? $params{Cipher}
                    : defined $key->Cipher    ? $key->Cipher
                    : 'Blowfish';

    # If they've given us no passphrase, we will be unencrypted.
    my $cipher_type = 0;

    if ($passphrase ne '') {
      $cipher_type = $CIPHERS_TEXT{ uc $cipher_name };
      croak "Unknown cipher: '$cipher_name'" unless defined $cipher_type;
    }

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
        my $cipher_names = $CIPHERS{$cipher_type};
        my $cipher_name;
        foreach my $name (@$cipher_names) {
          my $class = "Crypt::$name";
          (my $file = $class) =~ s=::|'=/=g;
          if ( eval { require "$file.pm"; 1 } ) {
            $cipher_name = $name; last;
          }
        }
        if (!defined $cipher_name) {
          croak "Unsupported cipher '$cipher_names->[0]': $@";
        }

        my $cipher = Crypt::CBC->new( -key    => $passphrase,
                                      -cipher => $cipher_name );
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
    deserialize() methods for SSH keys in the SSH1 format.

    Alternative formats (SSH2, PEM) are not implemented.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt> wrote the original version.

Dana Jacobsen E<lt>dana@acm.orgE<gt> wrote the new version.

=cut




1;
