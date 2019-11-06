package Crypt::OpenPGP::Certificate;
use strict;

use Crypt::OpenPGP::S2k;
use Crypt::OpenPGP::Key::Public;
use Crypt::OpenPGP::Key::Secret;
use Crypt::OpenPGP::Buffer;
use Crypt::OpenPGP::Util qw( mp2bin bin2mp bitsize );
use Crypt::OpenPGP::Constants qw( DEFAULT_CIPHER 
                                  PGP_PKT_PUBLIC_KEY
                                  PGP_PKT_PUBLIC_SUBKEY
                                  PGP_PKT_SECRET_KEY
                                  PGP_PKT_SECRET_SUBKEY );
use Crypt::OpenPGP::Cipher;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

{
    my @PKT_TYPES = (
        PGP_PKT_PUBLIC_KEY,
        PGP_PKT_PUBLIC_SUBKEY,
        PGP_PKT_SECRET_KEY,
        PGP_PKT_SECRET_SUBKEY
    );
    sub pkt_type {
        my $cert = shift;
        $PKT_TYPES[ ($cert->{is_secret} << 1) | $cert->{is_subkey} ];
    }
}

sub new {
    my $class = shift;
    my $cert = bless { }, $class;
    $cert->init(@_);
}

sub init {
    my $cert = shift;
    my %param = @_;
    if (my $key = $param{Key}) {
        $cert->{version} = $param{Version} || 4;
        $cert->{key} = $key;
        $cert->{is_secret} = $key->is_secret;
        $cert->{is_subkey} = $param{Subkey} || 0;
        $cert->{timestamp} = time;
        $cert->{pk_alg} = $key->alg_id;
        if ($cert->{version} < 4) {
            $cert->{validity} = $param{Validity} || 0;
            $key->alg eq 'RSA' or
                return (ref $cert)->error("Version 3 keys must be RSA");
        }
        $cert->{s2k} = Crypt::OpenPGP::S2k->new('Salt_Iter');

        if ($cert->{is_secret}) {
            $param{Passphrase} or
                return (ref $cert)->error("Need a Passphrase to lock key");
            $cert->{cipher} = $param{Cipher} || DEFAULT_CIPHER;
            $cert->lock($param{Passphrase});
        }
    }
    $cert;
}

sub type { $_[0]->{type} }
sub version { $_[0]->{version} }
sub timestamp { $_[0]->{timestamp} }
sub validity { $_[0]->{validity} }
sub pk_alg { $_[0]->{pk_alg} }
sub key { $_[0]->{key} }
sub is_secret { $_[0]->{key}->is_secret }
sub is_subkey { $_[0]->{is_subkey} }
sub is_protected { $_[0]->{is_protected} }
sub can_encrypt { $_[0]->{key}->can_encrypt }
sub can_sign { $_[0]->{key}->can_sign }
sub uid {
    my $cert = shift;
    $cert->{_uid} = shift if @_;
    $cert->{_uid};
}

sub public_cert {
    my $cert = shift;
    return $cert unless $cert->is_secret;
    my $pub = (ref $cert)->new;
    for my $f (qw( version timestamp pk_alg is_subkey )) {
        $pub->{$f} = $cert->{$f};
    }
    $pub->{validity} = $cert->{validity} if $cert->{version} < 4;
    $pub->{key} = $cert->{key}->public_key;
    $pub;
}

sub key_id {
    my $cert = shift;
    unless ($cert->{key_id}) {
        if ($cert->{version} < 4) {
            $cert->{key_id} = substr(mp2bin($cert->{key}->n), -8);
        }
        else {
            $cert->{key_id} = substr($cert->fingerprint, -8);
        }
    }
    $cert->{key_id};
}

sub key_id_hex { uc unpack 'H*', $_[0]->key_id }

sub fingerprint {
    my $cert = shift;
    unless ($cert->{fingerprint}) {
        if ($cert->{version} < 4) {
            my $dgst = Crypt::OpenPGP::Digest->new('MD5');
            $cert->{fingerprint} =
                $dgst->hash(mp2bin($cert->{key}->n) . mp2bin($cert->{key}->e));
        }
        else {
            my $data = $cert->public_cert->save;
            $cert->{fingerprint} = _gen_v4_fingerprint($data);
        }
    }
    $cert->{fingerprint};
}

sub fingerprint_hex { uc unpack 'H*', $_[0]->fingerprint }

sub fingerprint_words {
    require Crypt::OpenPGP::Words;
    Crypt::OpenPGP::Words->encode($_[0]->fingerprint);
}

sub _gen_v4_fingerprint {
    my($data) = @_;
    my $buf = Crypt::OpenPGP::Buffer->new;
    $buf->put_int8(0x99);
    $buf->put_int16(length $data);
    $buf->put_bytes($data);
    my $dgst = Crypt::OpenPGP::Digest->new('SHA1');
    $dgst->hash($buf->bytes);
}

sub parse {
    my $class = shift;
    my($buf, $secret, $subkey) = @_;
    my $cert = $class->new;
    $cert->{is_secret} = $secret;
    $cert->{is_subkey} = $subkey;

    $cert->{version} = $buf->get_int8;
    $cert->{timestamp} = $buf->get_int32;
    if ($cert->{version} < 4) {
        $cert->{validity} = $buf->get_int16;
    }
    $cert->{pk_alg} = $buf->get_int8;

    my $key_class = 'Crypt::OpenPGP::Key::' . ($secret ? 'Secret' : 'Public');
    my $key = $cert->{key} = $key_class->new($cert->{pk_alg}) or
        return $class->error("Key creation failed: " . $key_class->errstr);

    my @pub = $key->public_props;
    for my $e (@pub) {
        $key->$e($buf->get_mp_int);
    }

    if ($cert->{version} >= 4) {
        my $data = $buf->bytes(0, $buf->offset);
        $cert->{fingerprint} = _gen_v4_fingerprint($data);
    }

    if ($secret) {
        $cert->{cipher} = $buf->get_int8;
        if ($cert->{cipher}) {
            $cert->{is_protected} = 1;
            if ($cert->{cipher} == 255 || $cert->{cipher} == 254) {
                $cert->{sha1check} = $cert->{cipher} == 254;
                $cert->{cipher} = $buf->get_int8;
                $cert->{s2k} = Crypt::OpenPGP::S2k->parse($buf);
            }
            else {
                $cert->{s2k} = Crypt::OpenPGP::S2k->new('Simple');
                $cert->{s2k}->set_hash('MD5');
            }

            $cert->{iv} = $buf->get_bytes(8);
        }

        if ($cert->{is_protected}) {
            if ($cert->{version} < 4) {
                $cert->{encrypted} = {};
                my @sec = $key->secret_props;
                for my $e (@sec) {
                    my $h = $cert->{encrypted}{"${e}h"} = $buf->get_bytes(2);
                    $cert->{encrypted}{"${e}b"} =
                        $buf->get_bytes(int((unpack('n', $h)+7)/8));
                }
                $cert->{csum} = $buf->get_int16;
            }
            else {
                $cert->{encrypted} =
                    $buf->get_bytes($buf->length - $buf->offset);
            }
        }
        else {
            my @sec = $key->secret_props;
            for my $e (@sec) {
                $key->$e($buf->get_mp_int);
            }
        }
    }

    $cert;
}

sub save {
    my $cert = shift;
    my $buf = Crypt::OpenPGP::Buffer->new;

    $buf->put_int8($cert->{version});
    $buf->put_int32($cert->{timestamp});
    if ($cert->{version} < 4) {
        $buf->put_int16($cert->{validity});
    }
    $buf->put_int8($cert->{pk_alg});

    my $key = $cert->{key};
    my @pub = $key->public_props;
    for my $e (@pub) {
        $buf->put_mp_int($key->$e());
    }

    if ($cert->{key}->is_secret) {
        if ($cert->{cipher}) {
            $buf->put_int8(255);
            $buf->put_int8($cert->{cipher});
            $buf->append($cert->{s2k}->save);
            $buf->put_bytes($cert->{iv});

            if ($cert->{version} < 4) {
                my @sec = $key->secret_props;
                for my $e (@sec) {
                    $buf->put_bytes($cert->{encrypted}{"${e}h"});
                    $buf->put_bytes($cert->{encrypted}{"${e}b"});
                }
                $buf->put_int16($cert->{csum});
            }
            else {
                $buf->put_bytes($cert->{encrypted});
            }
        }
        else {
            my @sec = $key->secret_props;
            for my $e (@sec) {
                $key->$e($buf->get_mp_int);
            }
        }
    }
    $buf->bytes;
}

sub v3_checksum {
    my $cert = shift;
    my $k = $cert->{encrypted};
    my $sum = 0;
    my @sec = $cert->{key}->secret_props;
    for my $e (@sec) {
        $sum += unpack '%16C*', $k->{"${e}h"};
        $sum += unpack '%16C*', $k->{"${e}b"};
    }
    $sum & 0xFFFF;
}

sub unlock {
    my $cert = shift;
    return 1 unless $cert->{is_secret} && $cert->{is_protected};
    my($passphrase) = @_;
    my $cipher = Crypt::OpenPGP::Cipher->new($cert->{cipher}) or
        return $cert->error( Crypt::OpenPGP::Cipher->errstr );
    my $key = $cert->{s2k}->generate($passphrase, $cipher->keysize);
    $cipher->init($key, $cert->{iv});
    my @sec = $cert->{key}->secret_props;
    if ($cert->{version} < 4) {
        my $k = $cert->{encrypted};
        my $r = {};
        for my $e (@sec) {
            $r->{$e} = $k->{"${e}b"};
            $k->{"${e}b"} = $cipher->decrypt($r->{$e});
        }
        unless ($cert->{csum} == $cert->v3_checksum) {
            $k->{"${_}b"} = $r->{$_} for @sec;
            return $cert->error("Bad checksum");
        }
        for my $e (@sec) {
            $cert->{key}->$e(bin2mp($k->{"${e}b"}));
        }
        unless ($cert->{key}->check) {
            $k->{"${_}b"} = $r->{$_} for @sec;
            return $cert->error("p*q != n");
        }
    }
    else {
        my $decrypted = $cipher->decrypt($cert->{encrypted});
        if ($cert->{sha1check}) {
            my $dgst = Crypt::OpenPGP::Digest->new('SHA1');
            my $csum = substr $decrypted, -20, 20, '';
            unless ($dgst->hash($decrypted) eq $csum) {
                return $cert->error("Bad SHA-1 hash");
            }
        } else {
            my $csum = unpack "n", substr $decrypted, -2, 2, '';
            my $gen_csum = unpack '%16C*', $decrypted;
            unless ($csum == $gen_csum) {
               return $cert->error("Bad simple checksum");
            }
        }
        my $buf = Crypt::OpenPGP::Buffer->new;
        $buf->append($decrypted);
        for my $e (@sec) {
            $cert->{key}->$e( $buf->get_mp_int );
        }
    }

    $cert->{is_protected} = 0;

    1;
}

sub lock {
    my $cert = shift;
    return if !$cert->{is_secret} || $cert->{is_protected};
    my($passphrase) = @_;
    my $cipher = Crypt::OpenPGP::Cipher->new($cert->{cipher});
    my $sym_key = $cert->{s2k}->generate($passphrase, $cipher->keysize);
    $cert->{iv} = Crypt::OpenPGP::Util::get_random_bytes(8);
    $cipher->init($sym_key, $cert->{iv});
    my @sec = $cert->{key}->secret_props;
    if ($cert->{version} < 4) {
        my $k = $cert->{encrypted} = {};
        my $key = $cert->key;
        for my $e (@sec) {
            $k->{"${e}b"} = mp2bin($key->$e());
            $k->{"${e}h"} = pack 'n', bitsize($key->$e());
        }
        $cert->{csum} = $cert->v3_checksum;
        for my $e (@sec) {
            $k->{"${e}b"} = $cipher->encrypt( $k->{"${e}b"} );
        }
    }
    else {
        my $buf = Crypt::OpenPGP::Buffer->new;
        for my $e (@sec) {
            $buf->put_mp_int($cert->{key}->$e());
        }
        my $cnt = $buf->bytes;
        $cnt .= pack 'n', unpack '%16C*', $cnt;
        $cert->{encrypted} = $cipher->encrypt($cnt);
    }

    $cert->{is_protected} = 1;
    1;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Certificate - PGP Key certificate

=head1 SYNOPSIS

    use Crypt::OpenPGP::Certificate;

    my $dsa_secret_key = Crypt::OpenPGP::Key::Secret->new( 'DSA' );
    my $cert = Crypt::OpenPGP::Certificate->new(
        Key => $dsa_secret_key,
        Version => 4,
        Passphrase => 'foobar',
    );
    my $serialized = $cert->save;

    # Unlock the locked certificate (using the passphrase from above)
    $cert->unlock( 'foobar' );

=head1 DESCRIPTION

I<Crypt::OpenPGP::Certificate> encapsulates a PGP key certificate
for any underlying public-key algorithm, for public and secret keys,
and for master keys and subkeys. All of these scenarios are handled
by the same I<Certificate> class.

A I<Crypt::OpenPGP::Certificate> object wraps around a
I<Crypt::OpenPGP::Key> object; the latter implements all public-key
algorithm-specific functionality, while the certificate layer
manages some meta-data about the key, as well as the mechanisms
for locking and unlocking a secret key (using a passphrase).

=head1 USAGE

=head2 Crypt::OpenPGP::Certificate->new( %arg )

Constructs a new PGP key certificate object and returns that object.
If no arguments are provided in I<%arg>, the certificate is empty;
this is used in I<parse>, for example, to construct an empty object,
then fill it with the data in the buffer.

I<%arg> can contain:

=over 4

=item * Key

The public/secret key object, an object of type I<Crypt::OpenPGP::Key>.

This argument is required (for a non-empty certificate).

=item * Version

The certificate packet version, as defined in the OpenPGP RFC. The
two valid values are C<3> and C<4>.

This argument is optional; if not provided the default is to produce
version C<4> certificates. You may wish to override this for
compatibility with older versions of PGP.

=item * Subkey

A boolean flag: if true, indicates that this certificate is a subkey,
not a master key.

This argument is optional; the default value is C<0>.

=item * Validity

The number of days that this certificate is valid. This argument only
applies when creating a version 3 certificate; version 4 certificates
hold this information in a signature.

This argument is optional; the default value is C<0>, which means that
the certificate never expires.

=item * Passphrase

If you are creating a certificate for a secret key--indicated by whether
or not the I<Key> (above) is a secret key--you will need to lock it
(that is, encrypt the secret part of the key). The string provided in
I<Passphrase> is used as the passphrase to lock the key.

This argument is required if the certificate holds a secret key.

=item * Cipher

Specifies the symmetric cipher to use when locking (encrypting) the
secret part of a secret key. Valid values are any supported symmetric
cipher names, which can be found in I<Crypt::OpenPGP::Cipher>.

This argument is optional; if not specified, C<DES3> is used.

=back

=head2 $cert->save

Serializes the I<Crypt::OpenPGP::Certificate> object I<$cert> into a
string of octets, suitable for saving in a keyring file.

=head2 Crypt::OpenPGP::Certificate->parse($buffer)

Given I<$buffer>, a I<Crypt::OpenPGP::Buffer> object holding (or with
offset point to) a certificate packet, returns a new object of type
I<Crypt::OpenPGP::Certificate>, initialized with the data from the
buffer.

=head2 $cert->lock($passphrase)

Locks the secret key data by encrypting that data with I<$passphrase>.

Returns true on success, C<undef> on failure; in the case of failure
call I<errstr> to get the error message.

=head2 $cert->unlock($passphrase)

Uses the passphrase I<$passphrase> to unlock (decrypt) the secret
part of the key.

Returns true on success, C<undef> on failure; in the case of failure
call I<errstr> to get the error message.

=head2 $cert->fingerprint

Returns the key fingerprint as an octet string.

=head2 $cert->fingerprint_hex

Returns the key fingerprint as a hex string.

=head2 $cert->fingerprint_words

Returns the key fingerprint as a list of English words, where each word
represents one octet from the fingerprint. See I<Crypt::OpenPGP::Words>
for more details about the encoding.

=head2 $cert->key_id

Returns the key ID.

=head2 $cert->key_id_hex

Returns the key ID as a hex string.

=head2 $cert->key

Returns the algorithm-specific portion of the certificate, the public
or secret key object (an object of type I<Crypt::OpenPGP::Key>).

=head2 $cert->public_cert

Returns a public version of the certificate, with a public key. If
the certificate was already public, the same certificate is returned;
if it was a secret certificate, a new I<Crypt::OpenPGP::Certificate>
object is created, and the secret key is made into a public version
of the key.

=head2 $cert->version

Returns the version of the certificate (C<3> or C<4>).

=head2 $cert->timestamp

Returns the creation date and time (in epoch time).

=head2 $cert->validity

Returns the number of days that the certificate is valid for version
3 keys.

=head2 $cert->is_secret

Returns true if the certificate holds a secret key, false otherwise.

=head2 $cert->is_protected

Returns true if the certificate is locked, false otherwise.

=head2 $cert->is_subkey

Returns true if the certificate is a subkey, false otherwise.

=head2 $cert->can_encrypt

Returns true if the public key algorithm for the certificate I<$cert>
can perform encryption/decryption, false otherwise.

=head2 $cert->can_sign

Returns true if the public key algorithm for the certificate I<$cert>
can perform signing/verification, false otherwise.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
