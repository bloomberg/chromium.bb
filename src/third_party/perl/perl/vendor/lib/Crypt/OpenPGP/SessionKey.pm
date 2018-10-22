package Crypt::OpenPGP::SessionKey;
use strict;

use Crypt::OpenPGP::Constants qw( DEFAULT_CIPHER );
use Crypt::OpenPGP::Key::Public;
use Crypt::OpenPGP::Util qw( mp2bin bin2mp bitsize );
use Crypt::OpenPGP::Buffer;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub key_id { $_[0]->{key_id} }

sub new {
    my $class = shift;
    my $key = bless { }, $class;
    $key->init(@_);
}

sub init {
    my $key = shift;
    my %param = @_;
    $key->{version} = 3;
    if ((my $cert = $param{Key}) && (my $sym_key = $param{SymKey})) {
        my $alg = $param{Cipher} || DEFAULT_CIPHER;
        my $keysize = Crypt::OpenPGP::Cipher->new($alg)->keysize;
        $sym_key = substr $sym_key, 0, $keysize;
        my $pk = $cert->key->public_key;
        my $enc = $key->_encode($sym_key, $alg, $pk->bytesize) or
            return (ref $key)->error("Encoding symkey failed: " . $key->errstr);
        $key->{key_id} = $cert->key_id;
        $key->{C} = $pk->encrypt($enc) or
            return (ref $key)->error("Encryption failed: " . $pk->errstr);
        $key->{pk_alg} = $pk->alg_id;
    }
    $key;
}

sub parse {
    my $class = shift;
    my($buf) = @_;
    my $key = $class->new;
    $key->{version} = $buf->get_int8;
    return $class->error("Unsupported version ($key->{version})")
        unless $key->{version} == 2 || $key->{version} == 3;
    $key->{key_id} = $buf->get_bytes(8);
    $key->{pk_alg} = $buf->get_int8;
    my $pk = Crypt::OpenPGP::Key::Public->new($key->{pk_alg});
    my @props = $pk->crypt_props;
    for my $e (@props) {
        $key->{C}{$e} = $buf->get_mp_int;
    }
    $key;
}

sub save {
    my $key = shift;
    my $buf = Crypt::OpenPGP::Buffer->new;
    $buf->put_int8($key->{version});
    $buf->put_bytes($key->{key_id}, 8);
    $buf->put_int8($key->{pk_alg});
    my $c = $key->{C};
    for my $mp (values %$c) {
        $buf->put_mp_int($mp);
    }
    $buf->bytes;
}

sub display {
    my $key = shift;
    my $str = sprintf ":pubkey enc packet: version %d, algo %d, keyid %s\n",
        $key->{version}, $key->{pk_alg}, uc unpack('H*', $key->{key_id});
    for my $mp (values %{ $key->{C} }) {
        $str .= sprintf "        data: [%d bits]\n", bitsize($mp);
    }
    $str;
}

sub decrypt {
    my $key = shift;
    my($sk) = @_;
    return $key->error("Invalid secret key ID")
        unless $key->key_id eq $sk->key_id;
    my($sym_key, $alg) = __PACKAGE__->_decode($sk->key->decrypt($key->{C}))
        or return $key->error("Session key decryption failed: " .
            __PACKAGE__->errstr);
    ($sym_key, $alg);
}

sub _encode {
    my $class = shift;
    require Crypt::Random;
    my($sym_key, $sym_alg, $size) = @_;
    my $padlen = "$size" - length($sym_key) - 2 - 2 - 2;
    my $pad = Crypt::Random::makerandom_octet( Length => $padlen,
                                               Skip => chr(0) );
    bin2mp(pack 'na*na*n', 2, $pad, $sym_alg, $sym_key,
        unpack('%16C*', $sym_key));
}

sub _decode {
    my $class = shift;
    my($n) = @_;
    my $ser = mp2bin($n);
    return $class->error("Encoded data must start with 2")
        unless unpack('C', $ser) == 2;
    my $csum = unpack 'n', substr $ser, -2, 2, '';
    my($pad, $sym_key) = split /\0/, $ser, 2;
    my $sym_alg = ord substr $sym_key, 0, 1, '';
    return $class->error("Encoded data has bad checksum")
        unless unpack('%16C*', $sym_key) == $csum;
    ($sym_key, $sym_alg);
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::SessionKey - Encrypted Session Key

=head1 SYNOPSIS

    use Crypt::OpenPGP::SessionKey;

    my $public_key = Crypt::OpenPGP::Key::Public->new( 'RSA' );
    my $key_data = 'f' x 64;    ## Not a very good key :)

    my $skey = Crypt::OpenPGP::SessionKey->new(
        Key     => $public_key,
        SymKey  => $key_data,
    );
    my $serialized = $skey->save;

    my $secret_key = Crypt::OpenPGP::Key::Secret->new( 'RSA' );
    ( $key_data, my( $alg ) ) = $skey->decrypt( $secret_key );

=head1 DESCRIPTION

I<Crypt::OpenPGP::SessionKey> implements encrypted session key packets;
these packets store public-key-encrypted key data that, when decrypted
using the corresponding secret key, can be used to decrypt a block of
ciphertext--that is, a I<Crypt::OpenPGP::Ciphertext> object.

=head1 USAGE

=head2 Crypt::OpenPGP::SessionKey->new( %arg )

Creates a new encrypted session key packet object and returns that
object. If there are no arguments in I<%arg>, the object is created
empty; this is used, for example in I<parse> (below), to create an
empty packet which is then filled from the data in the buffer.

If you wish to initialize a non-empty object, I<%arg> can contain:

=over 4

=item * Key

A public key object; in other words, an object of a subclass of
I<Crypt::OpenPGP::Key::Private>. The public key is used to encrypt the
encoded session key such that it can only be decrypted by the secret
portion of the key.

This argument is required (for a non-empty object).

=item * SymKey

The symmetric cipher key: a string of octets that make up the key data
of the symmetric cipher key. This should be at least long enough for
the key length of your chosen cipher (see I<Cipher>, below), or, if
you have not specified a cipher, at least 64 bytes (to allow for long
cipher key sizes).

This argument is required (for a non-empty object).

=item * Cipher

The name (or ID) of a supported PGP cipher. See I<Crypt::OpenPGP::Cipher>
for a list of valid cipher names.

This argument is optional; by default I<Crypt::OpenPGP::Cipher> will
use C<DES3>.

=back

=head2 $skey->save

Serializes the session key packet and returns the string of octets.

=head2 Crypt::OpenPGP::SessionKey->parse($buffer)

Given I<$buffer>, a I<Crypt::OpenPGP::Buffer> object holding (or
with offset pointing to) an encrypted session key packet, returns
a new I<Crypt::OpenPGP::Ciphertext> object, initialized with the
data in the buffer.

=head2 $skey->decrypt($secret_key)

Given a secret key object I<$secret_key> (an object of a subclass of
I<Crypt::OpenPGP::Key::Public>), decrypts and decodes the encrypted
session key data. The key data includes the symmetric key itself,
along with a one-octet ID of the symmetric cipher used to encrypt
the message.

Returns a list containing two items: the symmetric key and the cipher
algorithm ID. These are suitable for passing off to the I<decrypt>
method of a I<Crypt::OpenPGP::Ciphertext> object to decrypt a block
of encrypted data.

=head2 $skey->key_id

Returns the key ID of the public key used to encrypt the session key;
this is necessary for finding the appropriate secret key to decrypt
the key.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
