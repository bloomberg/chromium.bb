package Crypt::OpenPGP::SKSessionKey;
use strict;

use Crypt::OpenPGP::Constants qw( DEFAULT_CIPHER );
use Crypt::OpenPGP::Buffer;
use Crypt::OpenPGP::S2k;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub new {
    my $class = shift;
    my $key = bless { }, $class;
    $key->init(@_);
}

sub init {
    my $key = shift;
    my %param = @_;
    $key->{version} = 4;
    if ((my $sym_key = $param{SymKey}) && (my $pass = $param{Passphrase})) {
        my $alg = $param{Cipher} || DEFAULT_CIPHER;
        my $cipher = Crypt::OpenPGP::Cipher->new($alg);
        my $keysize = $cipher->keysize;
        $key->{s2k_ciph} = $cipher->alg_id;
        $key->{s2k} = $param{S2k} || Crypt::OpenPGP::S2k->new('Salt_Iter');
        $sym_key = substr $sym_key, 0, $keysize;
        my $s2k_key = $key->{s2k}->generate($pass, $keysize);
        $cipher->init($s2k_key);
    }
    $key;
}

sub parse {
    my $class = shift;
    my($buf) = @_;
    my $key = $class->new;
    $key->{version} = $buf->get_int8;
    return $class->error("Unsupported version ($key->{version})")
        unless $key->{version} == 4;
    $key->{s2k_ciph} = $buf->get_int8;
    $key->{s2k} = Crypt::OpenPGP::S2k->parse($buf);
    if ($buf->offset < $buf->length) {
        $key->{encrypted} = $buf->get_bytes( $buf->length - $buf->offset );
    }
    $key;
}

sub save {
    my $key = shift;
    my $buf = Crypt::OpenPGP::Buffer->new;
    $buf->put_int8($key->{version});
    $buf->put_int8($key->{s2k_ciph});
    $buf->put_bytes( $key->{s2k}->save );
    $buf->bytes;
}

sub decrypt {
    my $key = shift;
    my($passphrase) = @_;
    my $cipher = Crypt::OpenPGP::Cipher->new($key->{s2k_ciph});
    my $keysize = $cipher->keysize;
    my $s2k_key = $key->{s2k}->generate($passphrase, $keysize);
    my($sym_key, $alg);
    if ($key->{encrypted}) {
        $cipher->init($s2k_key);
        $sym_key = $cipher->decrypt($key->{encrypted});
        $alg = ord substr $sym_key, 0, 1, '';
    } else {
        $sym_key = $s2k_key;
        $alg = $cipher->alg_id;
    }
    ($sym_key, $alg);
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::SKSessionKey - Symmetric-Key Encrypted Session Key

=head1 SYNOPSIS

    use Crypt::OpenPGP::SKSessionKey;

    my $passphrase = 'foobar';  # Not a very good passphrase
    my $key_data = 'f' x 64;    # Not a very good key

    my $skey = Crypt::OpenPGP::SKSessionKey->new(
        Passphrase  => $passphrase,
        SymKey      => $key_data,
    );
    my $serialized = $skey->save;

=head1 DESCRIPTION

I<Crypt::OpenPGP::SKSessionKey> implements symmetric-key encrypted
session key packets; these packets store symmetric-key-encrypted key data
that, when decrypted using the proper passphrase, can be used to decrypt a
block of ciphertext--that is, a I<Crypt::OpenPGP::Ciphertext> object.

Symmetric-key encrypted session key packets can work in two different
ways: in one scenario the passphrase you provide is used to encrypt
a randomly chosen string of key material; the key material is the key
that is actually used to encrypt the data packet, and the passphrase
just serves to encrypt the key material. This encrypted key material
is then serialized into the symmetric-key encrypted session key packet.

The other method of using this encryption form is to use the passphrase
directly to encrypt the data packet. In this scenario the need for any
additional key material goes away, because all the receiver needs is
the same passphrase that you have entered to encrypt the data.

At the moment I<Crypt::OpenPGP> really only supports the first
scenario; note also that the interface to I<new> may change in the
future when support for the second scenario is added.

=head1 USAGE

=head2 Crypt::OpenPGP::SKSessionKey->new( %arg )

Creates a new encrypted session key packet object and returns that
object. If there are no arguments in I<%arg>, the object is created
empty; this is used, for example in I<parse> (below), to create an
empty packet which is then filled from the data in the buffer.

If you wish to initialize a non-empty object, I<%arg> can contain:

=over 4

=item * Passphrase

An arbitrary-length passphrase; that is, a string of octets. The
passphrase is used to encrypt the actual session key such that it can
only be decrypted by supplying the correct passphrase.

This argument is required (for a non-empty object).

=item * SymKey

The symmetric cipher key: a string of octets that make up the key data
of the symmetric cipher key. This should be at least long enough for
the key length of your chosen cipher (see I<Cipher>, below), or, if
you have not specified a cipher, at least 64 bytes (to allow for long
cipher key sizes).

This argument is required (for a non-empty object).

=item * S2k

An object of type I<Crypt::OpenPGP::S2k> (or rather, of one of its
subclasses). If you use the passphrase directly to encrypt the data
packet (scenario one, above), you will probably be generating the
key material outside of this class, meaning that you will need to pass
in the I<S2k> object that was used to generate that key material from
the passphrase. This is the way to do that.

=item * Cipher

The name (or ID) of a supported PGP cipher. See I<Crypt::OpenPGP::Cipher>
for a list of valid cipher names.

This argument is optional; by default I<Crypt::OpenPGP::Cipher> will
use C<DES3>.

=back

=head2 $skey->save

Serializes the session key packet and returns the string of octets.

=head2 Crypt::OpenPGP::SKSessionKey->parse($buffer)

Given I<$buffer>, a I<Crypt::OpenPGP::Buffer> object holding (or
with offset pointing to) an encrypted session key packet, returns
a new I<Crypt::OpenPGP::Ciphertext> object, initialized with the
data in the buffer.

=head2 $skey->decrypt($passphrase)

Given a passphrase I<$passphrase>, decrypts the encrypted session key
data. The key data includes the symmetric key itself, along with a
one-octet ID of the symmetric cipher used to encrypt the message.

Returns a list containing two items: the symmetric key and the cipher
algorithm ID. These are suitable for passing off to the I<decrypt>
method of a I<Crypt::OpenPGP::Ciphertext> object to decrypt a block
of encrypted data.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
