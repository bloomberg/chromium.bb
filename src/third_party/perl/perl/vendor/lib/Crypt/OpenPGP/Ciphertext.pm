package Crypt::OpenPGP::Ciphertext;
use strict;

use Crypt::OpenPGP::Cipher;
use Crypt::OpenPGP::Constants qw( DEFAULT_CIPHER
                                  PGP_PKT_ENCRYPTED
                                  PGP_PKT_ENCRYPTED_MDC );
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

use constant MDC_TRAILER => chr(0xd3) . chr(0x14);

sub pkt_type { $_[0]->{is_mdc} ? PGP_PKT_ENCRYPTED_MDC : PGP_PKT_ENCRYPTED }

sub new {
    my $class = shift;
    my $enc = bless { }, $class;
    $enc->init(@_);
}

sub init {
    my $enc = shift;
    my %param = @_;
    if ((my $key = $param{SymKey}) && (my $data = $param{Data})) {
        $enc->{is_mdc} = $param{MDC} || 0;
        $enc->{version} = 1;
        require Crypt::Random;
        my $alg = $param{Cipher} || DEFAULT_CIPHER;
        my $cipher = Crypt::OpenPGP::Cipher->new($alg, $key);
        my $bs = $cipher->blocksize;
        my $pad = Crypt::Random::makerandom_octet( Length => $bs );
        $pad .= substr $pad, -2, 2;
        $enc->{ciphertext} = $cipher->encrypt($pad);
        $cipher->sync unless $enc->{is_mdc};
        $enc->{ciphertext} .= $cipher->encrypt($data);

        if ($enc->{is_mdc}) {
            require Crypt::OpenPGP::MDC;
            my $mdc = Crypt::OpenPGP::MDC->new(
                Data => $pad . $data . MDC_TRAILER );
            my $mdc_buf = Crypt::OpenPGP::PacketFactory->save($mdc);
            $enc->{ciphertext} .= $cipher->encrypt($mdc_buf);
        }
    }
    $enc;
}

sub parse {
    my $class = shift;
    my($buf, $is_mdc) = @_;
    my $enc = $class->new;
    $enc->{is_mdc} = $is_mdc;
    if ($is_mdc) {
        $enc->{version} = $buf->get_int8;
    }
    $enc->{ciphertext} = $buf->get_bytes($buf->length - $buf->offset);
    $enc;
}

sub save {
    my $enc = shift;
    my $buf = Crypt::OpenPGP::Buffer->new;
    if ($enc->{is_mdc}) {
        $buf->put_int8($enc->{version});
    }
    $buf->put_bytes($enc->{ciphertext});
    $buf->bytes;
}

sub display {
    my $enc = shift;
    my $str = ":encrypted data packet:\n" .
              "        length: " . length($enc->{ciphertext}) . "\n";
    if ($enc->{is_mdc}) {
        $str .= "        is_mdc: $enc->{version}\n";
    }
    $str;
}

sub decrypt {
    my $enc = shift;
    my($key, $sym_alg) = @_;
    my $cipher = Crypt::OpenPGP::Cipher->new($sym_alg, $key) or
        return $enc->error( Crypt::OpenPGP::Cipher->errstr );
    my $padlen = $cipher->blocksize + 2;
    my $pt = $enc->{prefix} =
        $cipher->decrypt(substr $enc->{ciphertext}, 0, $padlen);
    return $enc->error("Bad checksum")
        unless substr($pt, -4, 2) eq substr($pt, -2, 2);
    $cipher->sync unless $enc->{is_mdc};
    $pt = $cipher->decrypt(substr $enc->{ciphertext}, $padlen);
    if ($enc->{is_mdc}) {
        my $mdc_buf = Crypt::OpenPGP::Buffer->new_with_init(substr $pt,-22,22);
        $pt = substr $pt, 0, -22;
        my $mdc = Crypt::OpenPGP::PacketFactory->parse($mdc_buf);
        return $enc->error("Encrypted MDC packet without MDC")
            unless $mdc && ref($mdc) eq 'Crypt::OpenPGP::MDC';
        require Crypt::OpenPGP::Digest;
        my $dgst = Crypt::OpenPGP::Digest->new('SHA1');
        my $hash = $dgst->hash($enc->{prefix} . $pt . chr(0xd3) . chr(0x14));
        return $enc->error("SHA-1 hash of plaintext does not match MDC body")
            unless $mdc->digest eq $hash;
    }
    $pt;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Ciphertext - Encrypted data packet

=head1 SYNOPSIS

    use Crypt::OpenPGP::Ciphertext;

    my $key_data = 'f' x 64;    ## Not a very good key :)

    my $ct = Crypt::OpenPGP::Ciphertext->new(
        Data   => "foo bar baz",
        SymKey => $key_data,
    );
    my $serialized = $ct->save;

    my $buffer = Crypt::OpenPGP::Buffer->new;
    my $ct2 = Crypt::OpenPGP::Ciphertext->parse( $buffer );
    my $data = $ct->decrypt( $key_data );

=head1 DESCRIPTION

I<Crypt::OpenPGP::Ciphertext> implements symmetrically encrypted data
packets, providing both encryption and decryption functionality. Both
standard encrypted data packets and encrypted-MDC (modification
detection code) packets are supported by this class. In the first case,
the encryption used in the packets is a variant on standard CFB mode,
and is described in the OpenPGP RFC, in section 12.8 (OpenPGP CFB mode).
In the second case (encrypted-MDC packets), the encryption is performed
in standard CFB mode, without the special resync used in PGP's CFB.

=head1 USAGE

=head2 Crypt::OpenPGP::Ciphertext->new( %arg )

Creates a new symmetrically encrypted data packet object and returns
that object. If there are no arguments in I<%arg>, the object is
created with an empty data container; this is used, for example, in
I<parse> (below), to create an empty packet which is then filled from
the data in the buffer.

If you wish to initialize a non-empty object, I<%arg> can contain:

=over 4

=item * Data

A block of octets that make up the plaintext data to be encrypted.

This argument is required (for a non-empty object).

=item * SymKey

The symmetric cipher key: a string of octets that make up the key data
of the symmetric cipher key. This should be at least long enough for
the key length of your chosen cipher (see I<Cipher>, below), or, if
you have not specified a cipher, at least 64 bytes (to allow for
long cipher key sizes).

This argument is required (for a non-empty object).

=item * Cipher

The name (or ID) of a supported PGP cipher. See I<Crypt::OpenPGP::Cipher>
for a list of valid cipher names.

This argument is optional; by default I<Crypt::OpenPGP::Cipher> will
use C<DES3>.

=item * MDC

When set to a true value, encrypted texts will use encrypted MDC
(modification detection code) packets instead of standard encrypted data
packets. These are a newer form of encrypted data packets that
are followed by a C<SHA-1> hash of the plaintext data. This prevents
attacks that modify the encrypted text by using a message digest to
detect changes.

By default I<MDC> is set to C<0>, and encrypted texts use standard
encrypted data packets. Set it to a true value to turn on MDC packets.

=back

=head2 $ct->save

Returns the block of ciphertext created in I<new> (assuming that you
created a non-empty packet by specifying some data; otherwise returns
an empty string).

=head2 Crypt::OpenPGP::Ciphertext->parse($buffer)

Given I<$buffer>, a I<Crypt::OpenPGP::Buffer> object holding (or
with offset pointing to) a symmetrically encrypted data packet, returns
a new I<Crypt::OpenPGP::Ciphertext> object, initialized with the
ciphertext in the buffer.

=head2 $ct->decrypt($key, $alg)

Decrypts the ciphertext in the I<Crypt::OpenPGP::Ciphertext> object
and returns the plaintext. I<$key> is the encryption key, and I<$alg>
is the name (or ID) of the I<Crypt::OpenPGP::Cipher> type used to
encrypt the message. Obviously you can't just guess at these
parameters; this method (along with I<parse>, above) is best used along
with the I<Crypt::OpenPGP::SessionKey> object, which holds an encrypted
version of the key and cipher algorithm.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
