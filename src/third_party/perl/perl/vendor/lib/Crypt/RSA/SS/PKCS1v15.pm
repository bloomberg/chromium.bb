package Crypt::RSA::SS::PKCS1v15;
use strict;
use warnings;

## Crypt::RSA::SS:PKCS1v15
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.

use base 'Crypt::RSA::Errorhandler';
use Crypt::RSA::DataFormat qw(octet_len os2ip i2osp h2osp);
use Crypt::RSA::Primitives;
use Crypt::RSA::Debug qw(debug);
use Digest::SHA qw(sha1 sha224 sha256 sha384 sha512);
use Digest::MD5 qw(md5);
use Digest::MD2 qw(md2);

$Crypt::RSA::SS::PKCS1v15::VERSION = '1.99';

# See if we have a bug-fixed RIPEMD-160.
my $ripe_hash = undef;
if (eval { require Crypt::RIPEMD160; $Crypt::RIPEMD160::VERSION >= 0.05; }) {
  $ripe_hash = sub { my $r=new Crypt::RIPEMD160; $r->add(shift); $r->digest();};
}

sub new {

    my ($class, %params) = @_;
    my $self = bless {
                       primitives => new Crypt::RSA::Primitives,
                       digest     => $params{Digest} || 'SHA1',
                       encoding   => {
  # See http://rfc-ref.org/RFC-TEXTS/3447/chapter9.html
  MD2   =>[\&md2,   "30 20 30 0c 06 08 2a 86 48 86 f7 0d 02 02 05 00 04 10"],
  MD5   =>[\&md5,   "30 20 30 0c 06 08 2a 86 48 86 f7 0d 02 05 05 00 04 10"],
  SHA1  =>[\&sha1,  "30 21 30 09 06 05 2b 0e 03 02 1a 05 00 04 14"],
  SHA224=>[\&sha224,"30 2d 30 0d 06 09 60 86 48 01 65 03 04 02 04 05 00 04 1c"],
  SHA256=>[\&sha256,"30 31 30 0d 06 09 60 86 48 01 65 03 04 02 01 05 00 04 20"],
  SHA384=>[\&sha384,"30 41 30 0d 06 09 60 86 48 01 65 03 04 02 02 05 00 04 30"],
  SHA512=>[\&sha512,"30 51 30 0d 06 09 60 86 48 01 65 03 04 02 03 05 00 04 40"],
  RIPEMD160=>[$ripe_hash,"30 21 30 09 06 05 2B 24 03 02 01 05 00 04 14"],
                                     },
                       VERSION    => $Crypt::RSA::SS::PKCS1v15::VERSION,
                     }, $class;
    # Allow "sha256", "sha-256", "RipeMD-160", etc.
    $self->{digest} =~ tr/a-z/A-Z/;
    $self->{digest} =~ s/[^A-Z0-9]//g;
    if ($params{Version}) {
        # do versioning here
    }
    return $self;

}


sub sign {

    my ($self, %params) = @_;
    my $key = $params{Key};
    my $M = $params{Message} || $params{Plaintext};
    return $self->error ("No Message or Plaintext parameter", \$key, \%params) unless $M;
    return $self->error ("No Key parameter", \$M, \%params) unless $key;
    my $k = octet_len ($key->n);

    my $em;
    unless ($em = $self->encode ($M, $k)) {
        return $self->error ($self->errstr, \$key, \%params, \$M)
            if $self->errstr eq "Message too long.";
        return $self->error ("Modulus too short.", \$key, \%params, \$M)
            if $self->errstr eq "Intended encoded message length too short";
        # Other error
        return $self->error ($self->errstr, \$key, \%params, \$M);
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
    my $em = i2osp ($m, $k) ||
        return $self->error ("Invalid signature.", \$M, \$S, $key, \%params);
    my $em1;
    unless ($em1 = $self->encode ($M, $k)) {
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

    my $encoding = $self->{encoding}->{$self->{digest}};
    return $self->error ("Invalid encoding: $self->{digest}") unless defined $encoding;
    my ($hashfunc, $digestinfo) = @$encoding;
    return $self->error ("encoding method $self->{digest} not supported") unless defined $hashfunc;

    # Changed to match RFC 2313 (PKCS#1 v1.5) and 3447 (PKCS#1 v2.1).
    # There was apparently some confusion from XML documentation that
    # printed a different set of instructions here.  See, for example:
    #     http://osdir.com/ml/mozilla.crypto/2005-05/msg00300.htm
    # However, previously emlen was always k-1, so the result ended up
    # being identical anyway.  One change is that we now return if there
    # is not enough padding.  Previously the error string would be set
    # but processing would continue.
    #
    # Refs:
    #     http://rfc-ref.org/RFC-TEXTS/3447/chapter9.html
    #     https://tools.ietf.org/html/rfc2313
    my $H = $hashfunc->($M);
    my $alg = h2osp($digestinfo);
    return $self->error ("Invalid digest results: $self->{digest}") unless defined $H && length($H) > 0;

    my $T = $alg . $H;
    return $self->error ("Intended encoded message length too short.", \$M) if $emlen < length($T) + 11;
    my $pslen = $emlen - length($T) - 3;
    my $PS = chr(0xff) x $pslen;
    my $em = chr(0) . chr(1) . $PS . chr(0) . $T;
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
supported: MD2, MD5, SHA1, SHA224, SHA256, SHA384, and SHA512.
Digest defaults to SHA1.

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


