package Crypt::DSA;

use 5.006;
use strict;
use Digest::SHA1 qw( sha1 );
use Carp qw( croak );
use Crypt::DSA::KeyChain;
use Crypt::DSA::Key;
use Crypt::DSA::Signature;
use Crypt::DSA::Util qw( bitsize bin2mp mod_inverse mod_exp makerandom );

use vars qw( $VERSION );
BEGIN {
    $VERSION = '1.17';
}

sub new {
    my $class = shift;
    my $dsa = bless { @_ }, $class;
    $dsa->{_keychain} = Crypt::DSA::KeyChain->new(@_);
    $dsa;
}

sub keygen {
    my $dsa = shift;
    my $key = $dsa->{_keychain}->generate_params(@_);
    $dsa->{_keychain}->generate_keys($key);
    $key;
}

sub sign {
    my $dsa = shift;
    my %param = @_;
    my($key, $dgst);
    croak __PACKAGE__, "->sign: Need a Key" unless $key = $param{Key};
    unless ($dgst = $param{Digest}) {
        croak __PACKAGE__, "->sign: Need either Message or Digest"
            unless $param{Message};
        $dgst = sha1($param{Message});
    }
    my $dlen = length $dgst;

    my $i = bitsize($key->q) / 8;
    croak "Data too large for key size"
        if $dlen > $i || $dlen > 50;

    $dsa->_sign_setup($key)
        unless $key->kinv && $key->r;

    my $m = bin2mp($dgst);
    my $xr = ($key->priv_key * $key->r) % $key->q;
    my $s = $xr + $m;
    $s -= $key->q if $s > $key->q;
    $s = ($s * $key->kinv) % $key->q;

    my $sig = Crypt::DSA::Signature->new;
    $sig->r($key->r);
    $sig->s($s);
    $sig;
}

sub _sign_setup {
    my $dsa = shift;
    my $key = shift;
    my($k, $r);
    {
        $k = makerandom(Size => bitsize($key->q));
        $k -= $key->q if $k >= $key->q;
        redo if $k == 0;
    }
    $r = mod_exp($key->g, $k, $key->p);
    $r %= $key->q;
    my $kinv = mod_inverse($k, $key->q);
    $key->r($r);
    $key->kinv($kinv);
}

sub verify {
    my $dsa = shift;
    my %param = @_;
    my($key, $dgst, $sig);
    croak __PACKAGE__, "->verify: Need a Key" unless $key = $param{Key};
    unless ($dgst = $param{Digest}) {
        croak __PACKAGE__, "->verify: Need either Message or Digest"
            unless $param{Message};
        $dgst = sha1($param{Message});
    }
    croak __PACKAGE__, "->verify: Need a Signature"
        unless $sig = $param{Signature};
    my $u2 = mod_inverse($sig->s, $key->q);
    my $u1 = bin2mp($dgst);
    $u1 = ($u1 * $u2) % $key->q;
    $u2 = ($sig->r * $u2) % $key->q;
    my $t1 = mod_exp($key->g, $u1, $key->p);
    my $t2 = mod_exp($key->pub_key, $u2, $key->p);
    $u1 = ($t1 * $t2) % $key->p;
    $u1 %= $key->q;
    $u1 == $sig->r;
}

1;

__END__

=pod

=head1 NAME

Crypt::DSA - DSA Signatures and Key Generation

=head1 SYNOPSIS

    use Crypt::DSA;
    my $dsa = Crypt::DSA->new;

    my $key = $dsa->keygen(
                   Size      => 512,
                   Seed      => $seed,
                   Verbosity => 1
              );

    my $sig = $dsa->sign(
                   Message   => "foo bar",
                   Key       => $key
              );

    my $verified = $dsa->verify(
                   Message   => "foo bar",
                   Signature => $sig,
                   Key       => $key,
              );

=head1 DESCRIPTION

I<Crypt::DSA> is an implementation of the DSA (Digital Signature
Algorithm) signature verification system. The implementation
itself is pure Perl, although the heavy-duty mathematics underneath
are provided by the I<Math::Pari> library.

This package provides DSA signing, signature verification, and key
generation.

=head1 USAGE

The I<Crypt::DSA> public interface is similar to that of
I<Crypt::RSA>. This was done intentionally.

=head2 Crypt::DSA->new

Constructs a new I<Crypt::DSA> object. At the moment this isn't
particularly useful in itself, other than being the object you
need to do much else in the system.

Returns the new object.

=head2 $key = $dsa->keygen(%arg)

Generates a new set of DSA keys, including both the public and
private portions of the key.

I<%arg> can contain:

=over 4

=item * Size

The size in bits of the I<p> value to generate. The I<q> and
I<g> values are always 160 bits each.

This argument is mandatory.

=item * Seed

A seed with which I<q> generation will begin. If this seed does
not lead to a suitable prime, it will be discarded, and a new
random seed chosen in its place, until a suitable prime can be
found.

This is entirely optional, and if not provided a random seed will
be generated automatically.

=item * Verbosity

Should be either 0 or 1. A value of 1 will give you a progress
meter during I<p> and I<q> generation--this can be useful, since
the process can be relatively long.

The default is 0.

=back

=head2 $signature = $dsa->sign(%arg)

Signs a message (or the digest of a message) using the private
portion of the DSA key and returns the signature.

The return value--the signature--is a I<Crypt::DSA::Signature>
object.

I<%arg> can include:

=over 4

=item * Digest

A digest to be signed. The digest should be 20 bytes in length
or less.

You must provide either this argument or I<Message> (see below).

=item * Key

The I<Crypt::DSA::Key> object with which the signature will be
generated. Should contain a private key attribute (I<priv_key>).

This argument is required.

=item * Message

A plaintext message to be signed. If you provide this argument,
I<sign> will first produce a SHA1 digest of the plaintext, then
use that as the digest to sign. Thus writing

    my $sign = $dsa->sign(Message => $message, ... );

is a shorter way of writing

    use Digest::SHA1 qw( sha1 );
    my $sig = $dsa->sign(Digest => sha1( $message ), ... );

=back

=head2 $verified = $dsa->verify(%arg)

Verifies a signature generated with I<sign>. Returns a true
value on success and false on failure.

I<%arg> can contain:

=over 4

=item * Key

Key of the signer of the message; a I<Crypt::DSA::Key> object.
The public portion of the key is used to verify the signature.

This argument is required.

=item * Signature

The signature itself. Should be in the same format as returned
from I<sign>, a I<Crypt::DSA::Signature> object.

This argument is required.

=item * Digest

The original signed digest whose length is less than or equal to
20 bytes.

Either this argument or I<Message> (see below) must be present.

=item * Message

As above in I<sign>, the plaintext message that was signed, a
string of arbitrary length. A SHA1 digest of this message will
be created and used in the verification process.

=back

=head1 TODO

Add ability to munge format of keys. For example, read/write keys
from/to key files (SSH key files, etc.), and also write them in
other formats.

=head1 SUPPORT

Bugs should be reported via the CPAN bug tracker at

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Crypt-DSA>

For other issues, contact the author.

=head1 AUTHOR

Benjamin Trott E<lt>ben@sixapart.comE<gt>

=head1 COPYRIGHT

Except where otherwise noted,
Crypt::DSA is Copyright 2006 - 2011 Benjamin Trott.

Crypt::DSA is free software; you may redistribute it
and/or modify it under the same terms as Perl itself.

=cut
