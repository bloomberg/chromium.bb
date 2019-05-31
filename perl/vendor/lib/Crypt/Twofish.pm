# $Id: Twofish.pm,v 2.12 2001/05/21 17:38:01 ams Exp $
# Copyright 2001 Abhijit Menon-Sen <ams@toroid.org>

package Crypt::Twofish;

use strict;
use Carp;
use DynaLoader;
use vars qw( @ISA $VERSION );

@ISA = qw( DynaLoader );
$VERSION = '2.17';

bootstrap Crypt::Twofish $VERSION;

sub keysize   () { 16 }
sub blocksize () { 16 }

sub new
{
    my ($class, $key) = @_;

    croak "Usage: ".__PACKAGE__."->new(\$key)" unless $key;
    return Crypt::Twofish::setup($key);
}

sub encrypt
{
    my ($self, $data) = @_;

    croak "Usage: \$cipher->encrypt(\$data)" unless ref($self) && $data;
    $self->crypt($data, $data, 0);
}

sub decrypt
{
    my ($self, $data) = @_;

    croak "Usage: \$cipher->decrypt(\$data)" unless ref($self) && $data;
    $self->crypt($data, $data, 1);
}

# The functions below provide an interface that is call-compatible with
# the Crypt::Twofish 1.0 module. They do not, however, behave in exactly
# the same way: they don't pad keys, and cannot decrypt ciphertext which
# was written by the old module.
#
# (This interface is deprecated. Please use the documented interface
# instead).

sub Encipher
{
    my ($key, $keylength, $plaintext) = @_;

    require Crypt::CBC;
    my $cipher = Crypt::CBC->new($key, "Twofish");
    return $cipher->encrypt($plaintext);
}

sub Decipher
{
    my ($key, $keylength, $ciphertext, $cipherlength) = @_;

    require Crypt::CBC;
    my $cipher = Crypt::CBC->new($key, "Twofish");
    return $cipher->decrypt($ciphertext);
}

sub LastError    { "";    }
sub CheckTwofish { undef; }

1;

__END__

=head1 NAME

Crypt::Twofish - The Twofish Encryption Algorithm

=head1 SYNOPSIS

use Crypt::Twofish;

$cipher = Crypt::Twofish->new($key);

$ciphertext = $cipher->encrypt($plaintext);

$plaintext  = $cipher->decrypt($ciphertext);

=head1 DESCRIPTION

Twofish is a 128-bit symmetric block cipher with a variable length (128,
192, or 256-bit) key, developed by Counterpane Labs. It is unpatented
and free for all uses, as described at
<URL:http://www.counterpane.com/twofish.html>.

This module implements Twofish encryption. It supports the Crypt::CBC
interface, with the functions described below. It also provides an
interface that is call-compatible with Crypt::Twofish 1.0, but its use
in new code is strongly discouraged.

=head2 Functions

=over

=item blocksize

Returns the size (in bytes) of the block (16, in this case).

=item keysize

Returns the size (in bytes) of the key. Although the module understands
128, 192, and 256-bit keys, it returns 16 for compatibility with
Crypt::CBC.

=item new($key)

This creates a new Crypt::Twofish object with the specified key (which
should be 16, 24, or 32 bytes long).

=item encrypt($data)

Encrypts blocksize() bytes of $data and returns the corresponding
ciphertext.

=item decrypt($data)

Decrypts blocksize() bytes of $data and returns the corresponding
plaintext.

=back

=head1 SEE ALSO

Crypt::CBC, Crypt::Blowfish, Crypt::TEA

=head1 ACKNOWLEDGEMENTS

=over 4

=item Nishant Kakani

For writing Crypt::Twofish 1.0 (this version is a complete rewrite).

=item Tony Cook

For making the module work under Activeperl, testing on several
platforms, and suggesting that I probe for features via %Config.

=back

=head1 AUTHOR

Abhijit Menon-Sen <ams@toroid.org>

Copyright 2001 Abhijit Menon-Sen.

This module is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.
