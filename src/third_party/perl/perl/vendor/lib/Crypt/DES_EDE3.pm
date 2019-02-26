# $Id: DES_EDE3.pm,v 1.2 2001/09/15 03:41:09 btrott Exp $

package Crypt::DES_EDE3;
use strict;

use Crypt::DES;
use vars qw( $VERSION );
$VERSION = '0.01';

sub new {
    my $class = shift;
    my $ede3 = bless {}, $class;
    $ede3->init(@_);
}

sub keysize   { 24 }
sub blocksize { 8 }

sub init {
    my $ede3 = shift;
    my($key) = @_;
    for my $i (1..3) {
        $ede3->{"des$i"} = Crypt::DES->new(substr $key, 8*($i-1), 8);
    }
    $ede3;
}

sub encrypt {
    my($ede3, $block) = @_;
    $ede3->{des3}->encrypt(
        $ede3->{des2}->decrypt(
            $ede3->{des1}->encrypt($block)
        )
    );
}

sub decrypt {
    my($ede3, $block) = @_;
    $ede3->{des1}->decrypt(
        $ede3->{des2}->encrypt(
            $ede3->{des3}->decrypt($block)
        )
    );
}

1;
__END__

=head1 NAME

Crypt::DES_EDE3 - Triple-DES EDE encryption/decryption

=head1 SYNOPSIS

    use Crypt::DES_EDE3;
    my $ede3 = Crypt::DES_EDE3->new($key);
    $ede3->encrypt($block);

=head1 DESCRIPTION

I<Crypt::DES_EDE3> implements DES-EDE3 encryption. This is triple-DES
encryption where an encrypt operation is encrypt-decrypt-encrypt, and
decrypt is decrypt-encrypt-decrypt. This implementation uses I<Crypt::DES>
to do its dirty DES work, and simply provides a wrapper around that
module: setting up the individual DES ciphers, initializing the keys,
and performing the encryption/decryption steps.

DES-EDE3 encryption requires a key size of 24 bytes.

You're probably best off not using this module directly, as the I<encrypt>
and I<decrypt> methods expect 8-octet blocks. You might want to use the
module in conjunction with I<Crypt::CBC>, for example. This would be
DES-EDE3-CBC, or triple-DES in outer CBC mode.

=head1 USAGE

=head2 $ede3 = Crypt::DES_EDE3->new($key)

Creates a new I<Crypt::DES_EDE3> object (really, a collection of three DES
ciphers), and initializes each cipher with part of I<$key>, which should be
at least 24 bytes. If it's longer than 24 bytes, the extra bytes will be
ignored.

Returns the new object.

=head2 $ede3->encrypt($block)

Encrypts an 8-byte block of data I<$block> using the three DES ciphers in
an encrypt-decrypt-encrypt operation.

Returns the encrypted block.

=head2 $ede3->decrypt($block)

Decrypts an 8-byte block of data I<$block> using the three DES ciphers in
a decrypt-encrypt-decrypt operation.

Returns the decrypted block.

=head2 $ede3->blocksize

Returns the block size (8).

=head2 $ede3->keysize

Returns the key size (24).

=head1 LICENSE

Crypt::DES_EDE3 is free software; you may redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR & COPYRIGHTS

Crypt::DES_EDE3 is Copyright 2001 Benjamin Trott, ben@rhumba.pair.com. All
rights reserved.

=cut
