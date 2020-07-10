package Crypt::PK::ECC;

use strict;
use warnings;
our $VERSION = '0.063';

require Exporter; our @ISA = qw(Exporter); ### use Exporter 'import';
our %EXPORT_TAGS = ( all => [qw( ecc_encrypt ecc_decrypt ecc_sign_message ecc_verify_message ecc_sign_hash ecc_verify_hash ecc_shared_secret )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;
use Crypt::Digest qw(digest_data digest_data_b64u);
use Crypt::Misc qw(read_rawfile encode_b64u decode_b64u encode_b64 decode_b64 pem_to_der der_to_pem);
use Crypt::PK;

our %curve = ( # must be "our" as we use it from XS code
  # extra curves not recognized by libtomcrypt
  'wap-wsg-idm-ecid-wtls8' => {
        prime      => "FFFFFFFFFFFFFFFFFFFFFFFFFDE7",
        A          => "0000000000000000000000000000",
        B          => "0000000000000000000000000003",
        order      => "0100000000000001ECEA551AD837E9",
        Gx         => "0000000000000000000000000001",
        Gy         => "0000000000000000000000000002",
        cofactor   => 1,
        oid        => '2.23.43.1.4.8',
  },
  'wap-wsg-idm-ecid-wtls9' => {
        prime      => "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC808F",
        A          => "0000000000000000000000000000000000000000",
        B          => "0000000000000000000000000000000000000003",
        order      => "0100000000000000000001CDC98AE0E2DE574ABF33",
        Gx         => "0000000000000000000000000000000000000001",
        Gy         => "0000000000000000000000000000000000000002",
        cofactor   => 1,
        oid        => '2.23.43.1.4.9',
  },
  # some unusual openssl names
  "wap-wsg-idm-ecid-wtls6"  => 'secp112r1',
  "wap-wsg-idm-ecid-wtls7"  => 'secp160r2',
  "wap-wsg-idm-ecid-wtls12" => 'secp224r1',
  # extra aliases
  'P-256K'                  => 'secp256k1',
);

our %curve_oid2name = ( # must be "our" as we use it from XS code
  # the following are used to derive curve_name from OID in key2hash()
  "1.2.840.10045.3.1.1"   => "secp192r1",
  "1.2.840.10045.3.1.2"   => "prime192v2",
  "1.2.840.10045.3.1.3"   => "prime192v3",
  "1.2.840.10045.3.1.4"   => "prime239v1",
  "1.2.840.10045.3.1.5"   => "prime239v2",
  "1.2.840.10045.3.1.6"   => "prime239v3",
  "1.2.840.10045.3.1.7"   => "secp256r1",
  "1.3.132.0.6"           => "secp112r1",
  "1.3.132.0.7"           => "secp112r2",
  "1.3.132.0.8"           => "secp160r1",
  "1.3.132.0.9"           => "secp160k1",
  "1.3.132.0.10"          => "secp256k1",
  "1.3.132.0.28"          => "secp128r1",
  "1.3.132.0.29"          => "secp128r2",
  "1.3.132.0.30"          => "secp160r2",
  "1.3.132.0.31"          => "secp192k1",
  "1.3.132.0.32"          => "secp224k1",
  "1.3.132.0.33"          => "secp224r1",
  "1.3.132.0.34"          => "secp384r1",
  "1.3.132.0.35"          => "secp521r1",
  "1.3.36.3.3.2.8.1.1.1"  => "brainpoolp160r1",
  "1.3.36.3.3.2.8.1.1.2"  => "brainpoolp160t1",
  "1.3.36.3.3.2.8.1.1.3"  => "brainpoolp192r1",
  "1.3.36.3.3.2.8.1.1.4"  => "brainpoolp192t1",
  "1.3.36.3.3.2.8.1.1.5"  => "brainpoolp224r1",
  "1.3.36.3.3.2.8.1.1.6"  => "brainpoolp224t1",
  "1.3.36.3.3.2.8.1.1.7"  => "brainpoolp256r1",
  "1.3.36.3.3.2.8.1.1.8"  => "brainpoolp256t1",
  "1.3.36.3.3.2.8.1.1.9"  => "brainpoolp320r1",
  "1.3.36.3.3.2.8.1.1.10" => "brainpoolp320t1",
  "1.3.36.3.3.2.8.1.1.11" => "brainpoolp384r1",
  "1.3.36.3.3.2.8.1.1.12" => "brainpoolp384t1",
  "1.3.36.3.3.2.8.1.1.13" => "brainpoolp512r1",
  "1.3.36.3.3.2.8.1.1.14" => "brainpoolp512t1",
);

my %curve2jwk = (
  # necessary for conversion of curve_name_or_OID >> P-NNN
  '1.2.840.10045.3.1.1' => 'P-192', # secp192r1
  '1.3.132.0.33'        => 'P-224', # secp224r1
  '1.2.840.10045.3.1.7' => 'P-256', # secp256r1
  '1.3.132.0.10'        => 'P-256K',# secp256k1
  '1.3.132.0.34'        => 'P-384', # secp384r1
  '1.3.132.0.35'        => 'P-521', # secp521r1
  'nistp192'            => 'P-192',
  'nistp224'            => 'P-224',
  'nistp256'            => 'P-256',
  'nistp384'            => 'P-384',
  'nistp521'            => 'P-521',
  'prime192v1'          => 'P-192',
  'prime256v1'          => 'P-256',
  'secp192r1'           => 'P-192',
  'secp224r1'           => 'P-224',
  'secp256r1'           => 'P-256',
  'secp256k1'           => 'P-256K',
  'secp384r1'           => 'P-384',
  'secp521r1'           => 'P-521',
);

sub _import_hex {
  my ($self, $x, $y, $k, $crv) = @_;
  croak "FATAL: no curve" if !$crv;
  if (defined $k && length($k) > 0) {
    croak "FATAL: invalid length (k)" if length($k) % 2;
    return $self->import_key_raw(pack("H*", $k), $crv);
  }
  elsif (defined $x && defined $y) {
    croak "FATAL: invalid length (x)"   if length($x) % 2;
    croak "FATAL: invalid length (y)"   if length($y) % 2;
    croak "FATAL: invalid length (x,y)" if length($y) != length($x);
    my $pub_hex = "04" . $x . $y;
    return $self->import_key_raw(pack("H*", $pub_hex), $crv);
  }
}

sub new {
  my $self = shift->_new();
  return @_ > 0 ? $self->import_key(@_) : $self;
}

sub export_key_pem {
  my ($self, $type, $password, $cipher) = @_;
  local $SIG{__DIE__} = \&CryptX::_croak;
  my $key = $self->export_key_der($type||'');
  return unless $key;
  return der_to_pem($key, "EC PRIVATE KEY", $password, $cipher) if substr($type, 0, 7) eq 'private';
  return der_to_pem($key, "PUBLIC KEY") if substr($type,0, 6) eq 'public';
}

sub export_key_jwk {
  my ($self, $type, $wanthash) = @_;
  local $SIG{__DIE__} = \&CryptX::_croak;
  my $kh = $self->key2hash;
  $kh->{curve_oid}  = '' if !defined $kh->{curve_oid};
  $kh->{curve_name} = '' if !defined $kh->{curve_name};
  my $curve_jwt = $curve2jwk{$kh->{curve_oid}} || $curve2jwk{lc $kh->{curve_name}} || $kh->{curve_name};
  if ($type && $type eq 'private') {
    return unless $kh->{pub_x} && $kh->{pub_y} && $kh->{k};
    for (qw/pub_x pub_y k/) {
      $kh->{$_} = "0$kh->{$_}" if length($kh->{$_}) % 2;
    }
    # NOTE: x + y are not necessary in privkey
    # but they are used in https://tools.ietf.org/html/rfc7517#appendix-A.2
    my $hash = {
      kty => "EC", crv => $curve_jwt,
      x => encode_b64u(pack("H*", $kh->{pub_x})),
      y => encode_b64u(pack("H*", $kh->{pub_y})),
      d => encode_b64u(pack("H*", $kh->{k})),
    };
    return $wanthash ? $hash : CryptX::_encode_json($hash);
  }
  elsif ($type && $type eq 'public') {
    return unless $kh->{pub_x} && $kh->{pub_y};
    for (qw/pub_x pub_y/) {
      $kh->{$_} = "0$kh->{$_}" if length($kh->{$_}) % 2;
    }
    my $hash = {
      kty => "EC", crv => $curve_jwt,
      x => encode_b64u(pack("H*", $kh->{pub_x})),
      y => encode_b64u(pack("H*", $kh->{pub_y})),
    };
    return $wanthash ? $hash : CryptX::_encode_json($hash);
  }
}

sub export_key_jwk_thumbprint {
  my ($self, $hash_name) = @_;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $hash_name ||= 'SHA256';
  my $h = $self->export_key_jwk('public', 1);
  my $json = CryptX::_encode_json({crv=>$h->{crv}, kty=>$h->{kty}, x=>$h->{x}, y=>$h->{y}});
  return digest_data_b64u($hash_name, $json);
}

sub import_key {
  my ($self, $key, $password) = @_;
  local $SIG{__DIE__} = \&CryptX::_croak;
  croak "FATAL: undefined key" unless $key;

  # special case
  if (ref($key) eq 'HASH') {
    if (($key->{pub_x} && $key->{pub_y}) || $key->{k}) {
      # hash exported via key2hash
      my $curve_name = $key->{curve_name} || $key->{curve_oid};
      return $self->_import_hex($key->{pub_x}, $key->{pub_y}, $key->{k}, $curve_name);
    }
    if ($key->{crv} && $key->{kty} && $key->{kty} eq "EC" && ($key->{d} || ($key->{x} && $key->{y}))) {
      # hash with items corresponding to JSON Web Key (JWK)
      $key = {%$key}; # make a copy as we will modify it
      for (qw/x y d/) {
        $key->{$_} = eval { unpack("H*", decode_b64u($key->{$_})) } if exists $key->{$_};
      }
      # names P-192 P-224 P-256 P-384 P-521 are recognized by libtomcrypt
      return $self->_import_hex($key->{x}, $key->{y}, $key->{d}, $key->{crv});
    }
    croak "FATAL: unexpected ECC key hash";
  }

  my $data;
  if (ref($key) eq 'SCALAR') {
    $data = $$key;
  }
  elsif (-f $key) {
    $data = read_rawfile($key);
  }
  else {
    croak "FATAL: non-existing file '$key'";
  }
  croak "FATAL: invalid key data" unless $data;

  if ($data =~ /-----BEGIN (EC PRIVATE|EC PUBLIC|PUBLIC) KEY-----(.*?)-----END/sg) {
    $data = pem_to_der($data, $password);
    my $rv = eval { $self->_import($data) } || eval { $self->_import_old($data) };
    return $rv if $rv;
  }
  elsif ($data =~ /-----BEGIN PRIVATE KEY-----(.*?)-----END/sg) {
    $data = pem_to_der($data, $password);
    return $self->_import_pkcs8($data, $password);
  }
  elsif ($data =~ /-----BEGIN ENCRYPTED PRIVATE KEY-----(.*?)-----END/sg) {
    $data = pem_to_der($data, $password);
    return $self->_import_pkcs8($data, $password);
  }
  elsif ($data =~ /^\s*(\{.*?\})\s*$/s) {
    # JSON Web Key (JWK) - http://tools.ietf.org/html/draft-ietf-jose-json-web-key
    my $json = "$1";
    my $h = CryptX::_decode_json($json);
    if ($h && $h->{kty} eq "EC") {
      for (qw/x y d/) {
        $h->{$_} = eval { unpack("H*", decode_b64u($h->{$_})) } if exists $h->{$_};
      }
      # names P-192 P-224 P-256 P-384 P-521 are recognized by libtomcrypt
      return $self->_import_hex($h->{x}, $h->{y}, $h->{d}, $h->{crv});
    }
  }
  elsif ($data =~ /-----BEGIN CERTIFICATE-----(.*?)-----END CERTIFICATE-----/sg) {
    $data = pem_to_der($data);
    return $self->_import_x509($data);
  }
  elsif ($data =~ /---- BEGIN SSH2 PUBLIC KEY ----(.*?)---- END SSH2 PUBLIC KEY ----/sg) {
    $data = pem_to_der($data);
    my ($typ, $skip, $pubkey) = Crypt::PK::_ssh_parse($data);
    return $self->import_key_raw($pubkey, "$2") if $pubkey && $typ =~ /^ecdsa-(.+?)-(.*)$/;
  }
  elsif ($data =~ /(ecdsa-\S+)\s+(\S+)/) {
    $data = decode_b64("$2");
    my ($typ, $skip, $pubkey) = Crypt::PK::_ssh_parse($data);
    return $self->import_key_raw($pubkey, "$2") if $pubkey && $typ =~ /^ecdsa-(.+?)-(.*)$/;
  }
  else {
    my $rv = eval { $self->_import($data) }                  ||
             eval { $self->_import_old($data) }              ||
             eval { $self->_import_pkcs8($data, $password) } ||
             eval { $self->_import_x509($data) };
    return $rv if $rv;
  }
  croak "FATAL: invalid or unsupported EC key format";
}

sub curve2hash {
  my $self = shift;
  my $kh = $self->key2hash;
  return {
     prime    => $kh->{curve_prime},
     A        => $kh->{curve_A},
     B        => $kh->{curve_B},
     Gx       => $kh->{curve_Gx},
     Gy       => $kh->{curve_Gy},
     cofactor => $kh->{curve_cofactor},
     order    => $kh->{curve_order},
     oid      => $kh->{curve_oid},
  };
}

### FUNCTIONS

sub ecc_encrypt {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->encrypt(@_);
}

sub ecc_decrypt {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->decrypt(@_);
}

sub ecc_sign_message {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->sign_message(@_);
}

sub ecc_verify_message {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->verify_message(@_);
}

sub ecc_sign_hash {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->sign_hash(@_);
}

sub ecc_verify_hash {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->verify_hash(@_);
}

sub ecc_shared_secret {
  my ($privkey, $pubkey) = @_;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $privkey = __PACKAGE__->new($privkey) unless ref $privkey;
  $pubkey  = __PACKAGE__->new($pubkey)  unless ref $pubkey;
  carp "FATAL: invalid 'privkey' param" unless ref($privkey) eq __PACKAGE__ && $privkey->is_private;
  carp "FATAL: invalid 'pubkey' param"  unless ref($pubkey)  eq __PACKAGE__;
  return $privkey->shared_secret($pubkey);
}

sub CLONE_SKIP { 1 } # prevent cloning

1;

=pod

=head1 NAME

Crypt::PK::ECC - Public key cryptography based on EC

=head1 SYNOPSIS

 ### OO interface

 #Encryption: Alice
 my $pub = Crypt::PK::ECC->new('Bob_pub_ecc1.der');
 my $ct = $pub->encrypt("secret message");
 #
 #Encryption: Bob (received ciphertext $ct)
 my $priv = Crypt::PK::ECC->new('Bob_priv_ecc1.der');
 my $pt = $priv->decrypt($ct);

 #Signature: Alice
 my $priv = Crypt::PK::ECC->new('Alice_priv_ecc1.der');
 my $sig = $priv->sign_message($message);
 #
 #Signature: Bob (received $message + $sig)
 my $pub = Crypt::PK::ECC->new('Alice_pub_ecc1.der');
 $pub->verify_message($sig, $message) or die "ERROR";

 #Shared secret
 my $priv = Crypt::PK::ECC->new('Alice_priv_ecc1.der');
 my $pub = Crypt::PK::ECC->new('Bob_pub_ecc1.der');
 my $shared_secret = $priv->shared_secret($pub);

 #Key generation
 my $pk = Crypt::PK::ECC->new();
 $pk->generate_key('secp160r1');
 my $private_der = $pk->export_key_der('private');
 my $public_der = $pk->export_key_der('public');
 my $private_pem = $pk->export_key_pem('private');
 my $public_pem = $pk->export_key_pem('public');
 my $public_raw = $pk->export_key_raw('public');

 ### Functional interface

 #Encryption: Alice
 my $ct = ecc_encrypt('Bob_pub_ecc1.der', "secret message");
 #Encryption: Bob (received ciphertext $ct)
 my $pt = ecc_decrypt('Bob_priv_ecc1.der', $ct);

 #Signature: Alice
 my $sig = ecc_sign_message('Alice_priv_ecc1.der', $message);
 #Signature: Bob (received $message + $sig)
 ecc_verify_message('Alice_pub_ecc1.der', $sig, $message) or die "ERROR";

 #Shared secret
 my $shared_secret = ecc_shared_secret('Alice_priv_ecc1.der', 'Bob_pub_ecc1.der');

=head1 DESCRIPTION

The module provides a set of core ECC functions as well as implementation of ECDSA and ECDH.

Supports elliptic curves C<y^2 = x^3 + a*x + b> over prime fields C<Fp = Z/pZ> (binary fields not supported).

=head1 METHODS

=head2 new

 my $pk = Crypt::PK::ECC->new();
 #or
 my $pk = Crypt::PK::ECC->new($priv_or_pub_key_filename);
 #or
 my $pk = Crypt::PK::ECC->new(\$buffer_containing_priv_or_pub_key);

Support for password protected PEM keys

 my $pk = Crypt::PK::ECC->new($priv_pem_key_filename, $password);
 #or
 my $pk = Crypt::PK::ECC->new(\$buffer_containing_priv_pem_key, $password);

=head2 generate_key

Uses Yarrow-based cryptographically strong random number generator seeded with
random data taken from C</dev/random> (UNIX) or C<CryptGenRandom> (Win32).

 $pk->generate_key($curve_name);
 #or
 $pk->generate_key($hashref_with_curve_params);

The following predefined C<$curve_name> values are supported:

 # curves from http://www.ecc-brainpool.org/download/Domain-parameters.pdf
 'brainpoolp160r1'
 'brainpoolp192r1'
 'brainpoolp224r1'
 'brainpoolp256r1'
 'brainpoolp320r1'
 'brainpoolp384r1'
 'brainpoolp512r1'
 # curves from http://www.secg.org/collateral/sec2_final.pdf
 'secp112r1'
 'secp112r2'
 'secp128r1'
 'secp128r2'
 'secp160k1'
 'secp160r1'
 'secp160r2'
 'secp192k1'
 'secp192r1'   ... same as nistp192, prime192v1
 'secp224k1'
 'secp224r1'   ... same as nistp224
 'secp256k1'   ... used by Bitcoin
 'secp256r1'   ... same as nistp256, prime256v1
 'secp384r1'   ... same as nistp384
 'secp521r1'   ... same as nistp521
 #curves from http://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.186-4.pdf
 'nistp192'    ... same as secp192r1, prime192v1
 'nistp224'    ... same as secp224r1
 'nistp256'    ... same as secp256r1, prime256v1
 'nistp384'    ... same as secp384r1
 'nistp521'    ... same as secp521r1
 # curves from ANS X9.62
 'prime192v1'   ... same as nistp192, secp192r1
 'prime192v2'
 'prime192v3'
 'prime239v1'
 'prime239v2'
 'prime239v3'
 'prime256v1'   ... same as nistp256, secp256r1

Using custom curve parameters:

 $pk->generate_key({ prime    => 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFF',
                     A        => 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFC',
                     B        => '22123DC2395A05CAA7423DAECCC94760A7D462256BD56916',
                     Gx       => '7D29778100C65A1DA1783716588DCE2B8B4AEE8E228F1896',
                     Gy       => '38A90F22637337334B49DCB66A6DC8F9978ACA7648A943B0',
                     order    => 'FFFFFFFFFFFFFFFFFFFFFFFF7A62D031C83F4294F640EC13',
                     cofactor => 1 });

See L<http://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.186-4.pdf>, L<http://www.secg.org/collateral/sec2_final.pdf>, L<http://www.ecc-brainpool.org/download/Domain-parameters.pdf>

=head2 import_key

Loads private or public key in DER or PEM format.

 $pk->import_key($filename);
 #or
 $pk->import_key(\$buffer_containing_key);

Support for password protected PEM keys:

 $pk->import_key($filename, $password);
 #or
 $pk->import_key(\$buffer_containing_key, $password);

Loading private or public keys form perl hash:

 $pk->import_key($hashref);

 # the $hashref is either a key exported via key2hash
 $pk->import_key({
      curve_A        => "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFC",
      curve_B        => "1C97BEFC54BD7A8B65ACF89F81D4D4ADC565FA45",
      curve_bits     => 160,
      curve_bytes    => 20,
      curve_cofactor => 1,
      curve_Gx       => "4A96B5688EF573284664698968C38BB913CBFC82",
      curve_Gy       => "23A628553168947D59DCC912042351377AC5FB32",
      curve_order    => "0100000000000000000001F4C8F927AED3CA752257",
      curve_prime    => "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF",
      k              => "B0EE84A749FE95DF997E33B8F333E12101E824C3",
      pub_x          => "5AE1ACE3ED0AEA9707CE5C0BCE014F6A2F15023A",
      pub_y          => "895D57E992D0A15F88D6680B27B701F615FCDC0F",
 });

 # or with the curve defined just by name
 $pk->import_key({
      curve_name => "secp160r1",
      k          => "B0EE84A749FE95DF997E33B8F333E12101E824C3",
      pub_x      => "5AE1ACE3ED0AEA9707CE5C0BCE014F6A2F15023A",
      pub_y      => "895D57E992D0A15F88D6680B27B701F615FCDC0F",
 });

 # or a hash with items corresponding to JWK (JSON Web Key)
 $pk->import_key({
       kty => "EC",
       crv => "P-256",
       x   => "MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4",
       y   => "4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM",
       d   => "870MB6gfuTJ4HtUnUvYMyJpr5eUZNP4Bk43bVdj3eAE",
 });

Supported key formats:

 # all formats can be loaded from a file
 my $pk = Crypt::PK::ECC->new($filename);

 # or from a buffer containing the key
 my $pk = Crypt::PK::ECC->new(\$buffer_with_key);

=over

=item * EC private keys with with all curve parameters

 -----BEGIN EC PRIVATE KEY-----
 MIIB+gIBAQQwCKEAcA6cIt6CGfyLKm57LyXWv2PgTjydrHSbvhDJTOl+7bzUW8DS
 rgSdtSPONPq1oIIBWzCCAVcCAQEwPAYHKoZIzj0BAQIxAP//////////////////
 ///////////////////////+/////wAAAAAAAAAA/////zB7BDD/////////////
 /////////////////////////////v////8AAAAAAAAAAP////wEMLMxL6fiPufk
 mI4Fa+P4LRkYHZxu/oFBEgMUCI9QE4daxlY5jYou0Z0qhcjt0+wq7wMVAKM1kmqj
 GaJ6HQCJamdzpIJ6zaxzBGEEqofKIr6LBTeOscce8yCtdG4dO2KLp5uYWfdB4IJU
 KjhVAvJdv1UpbDpUXjhydgq3NhfeSpYmLG9dnpi/kpLcKfj0Hb0omhR86doxE7Xw
 uMAKYLHOHX6BnXpDHXyQ6g5fAjEA////////////////////////////////x2NN
 gfQ3Ld9YGg2ySLCneuzsGWrMxSlzAgEBoWQDYgAEeGyHPLmHcszPQ9MIIYnznpzi
 QbvuJtYSjCqtIGxDfzgcLcc3nCc5tBxo+qX6OJEzcWdDAC0bwplY+9Z9jHR3ylNy
 ovlHoK4ItdWkVO8NH89SLSRyVuOF8N5t3CHIo93B
 -----END EC PRIVATE KEY-----

=item * EC private keys with curve defined by OID (short form)

 -----BEGIN EC PRIVATE KEY-----
 MHcCAQEEIBG1c3z52T8XwMsahGVdOZWgKCQJfv+l7djuJjgetdbDoAoGCCqGSM49
 AwEHoUQDQgAEoBUyo8CQAFPeYPvv78ylh5MwFZjTCLQeb042TjiMJxG+9DLFmRSM
 lBQ9T/RsLLc+PmpB1+7yPAR+oR5gZn3kJQ==
 -----END EC PRIVATE KEY-----

=item * EC private keys with curve defined by OID + compressed form (supported since: CryptX-0.059)

 -----BEGIN EC PRIVATE KEY-----
 MFcCAQEEIBG1c3z52T8XwMsahGVdOZWgKCQJfv+l7djuJjgetdbDoAoGCCqGSM49
 AwEHoSQDIgADoBUyo8CQAFPeYPvv78ylh5MwFZjTCLQeb042TjiMJxE=
 -----END EC PRIVATE KEY-----

=item * EC private keys in password protected PEM format

 -----BEGIN EC PRIVATE KEY-----
 Proc-Type: 4,ENCRYPTED
 DEK-Info: AES-128-CBC,98245C830C9282F7937E13D1D5BA11EC

 0Y85oZ2+BKXYwrkBjsZdj6gnhOAfS5yDVmEsxFCDug+R3+Kw3QvyIfO4MVo9iWoA
 D7wtoRfbt2OlBaLVl553+6QrUoa2DyKf8kLHQs1x1/J7tJOMM4SCXjlrOaToQ0dT
 o7fOnjQjHne16pjgBVqGilY/I79Ab85AnE4uw7vgEucBEiU0d3nrhwuS2Opnhzyx
 009q9VLDPwY2+q7tXjTqnk9mCmQgsiaDJqY09wlauSukYPgVuOJFmi1VdkRSDKYZ
 rUUsQvz6Q6Q+QirSlfHna+NhUgQ2eyhGszwcP6NU8iqIxI+NCwfFVuAzw539yYwS
 8SICczoC/YRlaclayXuomQ==
 -----END EC PRIVATE KEY-----

=item * EC public keys with all curve parameters

 -----BEGIN PUBLIC KEY-----
 MIH1MIGuBgcqhkjOPQIBMIGiAgEBMCwGByqGSM49AQECIQD/////////////////
 ///////////////////+///8LzAGBAEABAEHBEEEeb5mfvncu6xVoGKVzocLBwKb
 /NstzijZWfKBWxb4F5hIOtp3JqPEZV2k+/wOEQio/Re0SKaFVBmcR9CP+xDUuAIh
 AP////////////////////66rtzmr0igO7/SXozQNkFBAgEBA0IABITjF/nKK3jg
 pjmBRXKWAv7ekR1Ko/Nb5FFPHXjH0sDrpS7qRxFALwJHv7ylGnekgfKU3vzcewNs
 lvjpBYt0Yg4=
 -----END PUBLIC KEY-----

=item * EC public keys with curve defined by OID (short form)

 -----BEGIN PUBLIC KEY-----
 MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEoBUyo8CQAFPeYPvv78ylh5MwFZjT
 CLQeb042TjiMJxG+9DLFmRSMlBQ9T/RsLLc+PmpB1+7yPAR+oR5gZn3kJQ==
 -----END PUBLIC KEY-----

=item * EC public keys with curve defined by OID + public point in compressed form (supported since: CryptX-0.059)

 -----BEGIN PUBLIC KEY-----
 MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgADoBUyo8CQAFPeYPvv78ylh5MwFZjT
 CLQeb042TjiMJxE=
 -----END PUBLIC KEY-----

=item * PKCS#8 private keys with all curve parameters

 -----BEGIN PRIVATE KEY-----
 MIIBMAIBADCB0wYHKoZIzj0CATCBxwIBATAkBgcqhkjOPQEBAhkA////////////
 /////////v//////////MEsEGP////////////////////7//////////AQYIhI9
 wjlaBcqnQj2uzMlHYKfUYiVr1WkWAxUAxGloRDXes3jEtlypWR4qV2MFmi4EMQR9
 KXeBAMZaHaF4NxZYjc4ri0rujiKPGJY4qQ8iY3M3M0tJ3LZqbcj5l4rKdkipQ7AC
 GQD///////////////96YtAxyD9ClPZA7BMCAQEEVTBTAgEBBBiKolTGIsTgOCtl
 6dpdos0LvuaExCDFyT6hNAMyAAREwaCX0VY1LZxLW3G75tmft4p9uhc0J7/+NGaP
 DN3Tr7SXkT9+co2a+8KPJhQy10k=
 -----END PRIVATE KEY-----

=item * PKCS#8 private keys with curve defined by OID (short form)

 -----BEGIN PRIVATE KEY-----
 MG8CAQAwEwYHKoZIzj0CAQYIKoZIzj0DAQMEVTBTAgEBBBjFP/caeQV4WO3fnWWS
 f917PGzwtypd/t+hNAMyAATSg6pBT7RO6l/p+aKcrFsGuthUdfwJWS5V3NGcVt1b
 lEHQYjWya2YnHaPq/iMFa7A=
 -----END PRIVATE KEY-----

=item * PKCS#8 encrypted private keys - password protected keys (supported since: CryptX-0.059)

 -----BEGIN ENCRYPTED PRIVATE KEY-----
 MIGYMBwGCiqGSIb3DQEMAQMwDgQINApjTa6oFl0CAggABHi+59l4d4e6KtG9yci2
 BSC65LEsQSnrnFAExfKptNU1zMFsDLCRvDeDQDbxc6HlfoxyqFL4SmH1g3RvC/Vv
 NfckdL5O2L8MRnM+ljkFtV2Te4fszWcJFdd7KiNOkPpn+7sWLfzQdvhHChLKUzmz
 4INKZyMv/G7VpZ0=
 -----END ENCRYPTED PRIVATE KEY-----

=item * EC public key from X509 certificate

 -----BEGIN CERTIFICATE-----
 MIIBdDCCARqgAwIBAgIJAL2BBClDEnnOMAoGCCqGSM49BAMEMBcxFTATBgNVBAMM
 DFRlc3QgQ2VydCBFQzAgFw0xNzEyMzAyMDMzNDFaGA8zMDE3MDUwMjIwMzM0MVow
 FzEVMBMGA1UEAwwMVGVzdCBDZXJ0IEVDMFYwEAYHKoZIzj0CAQYFK4EEAAoDQgAE
 KvkL2r5xZp7RzxLQJK+6tn/7lic+L70e1fmNbHOdxRaRvbK5G0AQWrdsbjJb92Ni
 lCQk2+w/i+VuS2Q3MSR5TaNQME4wHQYDVR0OBBYEFGbJkDyKgaMcIGHS8/WuqIVw
 +R8sMB8GA1UdIwQYMBaAFGbJkDyKgaMcIGHS8/WuqIVw+R8sMAwGA1UdEwQFMAMB
 Af8wCgYIKoZIzj0EAwQDSAAwRQIhAJtOsmrM+gJpImoynAyqTN+7myL71uxd+YeC
 6ze4MnzWAiBQi5/BqEr/SQ1+BC2TPtswvJPRFh2ZvT/6Km3gKoNVXQ==
 -----END CERTIFICATE-----

=item * SSH public EC keys

 ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNT...T3xYfJIs=

=item * SSH public EC keys (RFC-4716 format)

 ---- BEGIN SSH2 PUBLIC KEY ----
 Comment: "521-bit ECDSA, converted from OpenSSH"
 AAAAE2VjZHNhLXNoYTItbmlzdHA1MjEAAAAIbmlzdHA1MjEAAACFBAFk35srteP9twCwYK
 vU9ovMBi77Dd6lEBPrFaMEb0CZdZ5MC3nSqflGHRWkSbUpjdPdO7cYQNpK9YXHbNSO5hbU
 1gFZgyiGFxwJYYz8NAjedBXMgyH4JWplK5FQm5P5cvaglItC9qkKioUXhCc67YMYBtivXl
 Ue0PgIq6kbHTqbX6+5Nw==
 ---- END SSH2 PUBLIC KEY ----

=item * EC private keys in JSON Web Key (JWK) format

See L<http://tools.ietf.org/html/draft-ietf-jose-json-web-key>

 {
  "kty":"EC",
  "crv":"P-256",
  "x":"MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4",
  "y":"4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM",
  "d":"870MB6gfuTJ4HtUnUvYMyJpr5eUZNP4Bk43bVdj3eAE",
 }

B<BEWARE:> For JWK support you need to have L<JSON::PP>, L<JSON::XS> or L<Cpanel::JSON::XS> module.

=item * EC public keys in JSON Web Key (JWK) format

 {
  "kty":"EC",
  "crv":"P-256",
  "x":"MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4",
  "y":"4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM",
 }

B<BEWARE:> For JWK support you need to have L<JSON::PP>, L<JSON::XS> or L<Cpanel::JSON::XS> module.

=back

=head2 import_key_raw

Import raw public/private key - can load data exported by L</export_key_raw>.

 $pk->import_key_raw($key, $curve);
 # $key .... data exported by export_key_raw()
 # $curve .. curve name or hashref with curve parameters - same as by generate_key()

=head2 export_key_der

 my $private_der = $pk->export_key_der('private');
 #or
 my $public_der = $pk->export_key_der('public');

Since CryptX-0.36 C<export_key_der> can also export keys in a format
that does not explicitly contain curve parameters but only curve OID.

 my $private_der = $pk->export_key_der('private_short');
 #or
 my $public_der = $pk->export_key_der('public_short');

Since CryptX-0.59 C<export_key_der> can also export keys in "compressed" format
that defines curve by OID + stores public point in compressed form.

 my $private_pem = $pk->export_key_der('private_compressed');
 #or
 my $public_pem = $pk->export_key_der('public_compressed');

=head2 export_key_pem

 my $private_pem = $pk->export_key_pem('private');
 #or
 my $public_pem = $pk->export_key_pem('public');

Since CryptX-0.36 C<export_key_pem> can also export keys in a format
that does not explicitly contain curve parameters but only curve OID.

 my $private_pem = $pk->export_key_pem('private_short');
 #or
 my $public_pem = $pk->export_key_pem('public_short');

Since CryptX-0.59 C<export_key_pem> can also export keys in "compressed" format
that defines curve by OID + stores public point in compressed form.

 my $private_pem = $pk->export_key_pem('private_compressed');
 #or
 my $public_pem = $pk->export_key_pem('public_compressed');

Support for password protected PEM keys

 my $private_pem = $pk->export_key_pem('private', $password);
 #or
 my $private_pem = $pk->export_key_pem('private', $password, $cipher);

 # supported ciphers: 'DES-CBC'
 #                    'DES-EDE3-CBC'
 #                    'SEED-CBC'
 #                    'CAMELLIA-128-CBC'
 #                    'CAMELLIA-192-CBC'
 #                    'CAMELLIA-256-CBC'
 #                    'AES-128-CBC'
 #                    'AES-192-CBC'
 #                    'AES-256-CBC' (DEFAULT)

=head2 export_key_jwk

I<Since: CryptX-0.022>

Exports public/private keys as a JSON Web Key (JWK).

 my $private_json_text = $pk->export_key_jwk('private');
 #or
 my $public_json_text = $pk->export_key_jwk('public');

Also exports public/private keys as a perl HASH with JWK structure.

 my $jwk_hash = $pk->export_key_jwk('private', 1);
 #or
 my $jwk_hash = $pk->export_key_jwk('public', 1);

B<BEWARE:> For JWK support you need to have L<JSON::PP>, L<JSON::XS> or L<Cpanel::JSON::XS> module.

=head2 export_key_jwk_thumbprint

I<Since: CryptX-0.031>

Exports the key's JSON Web Key Thumbprint as a string.

If you don't know what this is, see RFC 7638 L<https://tools.ietf.org/html/rfc7638>.

 my $thumbprint = $pk->export_key_jwk_thumbprint('SHA256');

=head2 export_key_raw

Export raw public/private key. Public key is exported in ASN X9.62 format (compressed or uncompressed),
private key is exported as raw bytes (padded with leading zeros to have the same size as the ECC curve).

 my $pubkey_octets  = $pk->export_key_raw('public');
 #or
 my $pubckey_octets = $pk->export_key_raw('public_compressed');
 #or
 my $privkey_octets = $pk->export_key_raw('private');

=head2 encrypt

 my $pk = Crypt::PK::ECC->new($pub_key_filename);
 my $ct = $pk->encrypt($message);
 #or
 my $ct = $pk->encrypt($message, $hash_name);

 #NOTE: $hash_name can be 'SHA1' (DEFAULT), 'SHA256' or any other hash supported by Crypt::Digest

=head2 decrypt

 my $pk = Crypt::PK::ECC->new($priv_key_filename);
 my $pt = $pk->decrypt($ciphertext);

=head2 sign_message

 my $pk = Crypt::PK::ECC->new($priv_key_filename);
 my $signature = $priv->sign_message($message);
 #or
 my $signature = $priv->sign_message($message, $hash_name);

 #NOTE: $hash_name can be 'SHA1' (DEFAULT), 'SHA256' or any other hash supported by Crypt::Digest

=head2 sign_message_rfc7518

I<Since: CryptX-0.024>

Same as L<sign_message|/sign_message> only the signature format is as defined by L<https://tools.ietf.org/html/rfc7518>
(JWA - JSON Web Algorithms).

B<BEWARE:> This creates signatures according to the structure that RFC 7518 describes but does not apply
the RFC logic for the hashing algorithm selection. You'll still need to specify, e.g., SHA256 for a P-256 key
to get a fully RFC-7518-compliant signature.

=head2 verify_message

 my $pk = Crypt::PK::ECC->new($pub_key_filename);
 my $valid = $pub->verify_message($signature, $message)
 #or
 my $valid = $pub->verify_message($signature, $message, $hash_name);

 #NOTE: $hash_name can be 'SHA1' (DEFAULT), 'SHA256' or any other hash supported by Crypt::Digest

=head2 verify_message_rfc7518

I<Since: CryptX-0.024>

Same as L<verify_message|/verify_message> only the signature format is as defined by L<https://tools.ietf.org/html/rfc7518>
(JWA - JSON Web Algorithms).

B<BEWARE:> This verifies signatures according to the structure that RFC 7518 describes but does not apply
the RFC logic for the hashing algorithm selection. You'll still need to specify, e.g., SHA256 for a P-256 key
to get a fully RFC-7518-compliant signature.

=head2 sign_hash

 my $pk = Crypt::PK::ECC->new($priv_key_filename);
 my $signature = $priv->sign_hash($message_hash);

=head2 sign_hash_rfc7518

I<Since: CryptX-0.059>

Same as L<sign_hash|/sign_hash> only the signature format is as defined by L<https://tools.ietf.org/html/rfc7518>
(JWA - JSON Web Algorithms).

=head2 verify_hash

 my $pk = Crypt::PK::ECC->new($pub_key_filename);
 my $valid = $pub->verify_hash($signature, $message_hash);

=head2 verify_hash_rfc7518

I<Since: CryptX-0.059>

Same as L<verify_hash|/verify_hash> only the signature format is as defined by L<https://tools.ietf.org/html/rfc7518>
(JWA - JSON Web Algorithms).

=head2 shared_secret

  # Alice having her priv key $pk and Bob's public key $pkb
  my $pk  = Crypt::PK::ECC->new($priv_key_filename);
  my $pkb = Crypt::PK::ECC->new($pub_key_filename);
  my $shared_secret = $pk->shared_secret($pkb);

  # Bob having his priv key $pk and Alice's public key $pka
  my $pk = Crypt::PK::ECC->new($priv_key_filename);
  my $pka = Crypt::PK::ECC->new($pub_key_filename);
  my $shared_secret = $pk->shared_secret($pka);  # same value as computed by Alice

=head2 is_private

 my $rv = $pk->is_private;
 # 1 .. private key loaded
 # 0 .. public key loaded
 # undef .. no key loaded

=head2 size

 my $size = $pk->size;
 # returns key size in bytes or undef if no key loaded

=head2 key2hash

 my $hash = $pk->key2hash;

 # returns hash like this (or undef if no key loaded):
 {
   size           => 20, # integer: key (curve) size in bytes
   type           => 1,  # integer: 1 .. private, 0 .. public
   #curve parameters
   curve_A        => "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFC",
   curve_B        => "1C97BEFC54BD7A8B65ACF89F81D4D4ADC565FA45",
   curve_bits     => 160,
   curve_bytes    => 20,
   curve_cofactor => 1,
   curve_Gx       => "4A96B5688EF573284664698968C38BB913CBFC82",
   curve_Gy       => "23A628553168947D59DCC912042351377AC5FB32",
   curve_name     => "secp160r1",
   curve_order    => "0100000000000000000001F4C8F927AED3CA752257",
   curve_prime    => "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF",
   #private key
   k              => "B0EE84A749FE95DF997E33B8F333E12101E824C3",
   #public key point coordinates
   pub_x          => "5AE1ACE3ED0AEA9707CE5C0BCE014F6A2F15023A",
   pub_y          => "895D57E992D0A15F88D6680B27B701F615FCDC0F",
 }

=head2 curve2hash

I<Since: CryptX-0.024>

 my $crv = $pk->curve2hash;

 # returns a hash that can be passed to: $pk->generate_key($crv)
 {
   A        => "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFC",
   B        => "1C97BEFC54BD7A8B65ACF89F81D4D4ADC565FA45",
   cofactor => 1,
   Gx       => "4A96B5688EF573284664698968C38BB913CBFC82",
   Gy       => "23A628553168947D59DCC912042351377AC5FB32",
   order    => "0100000000000000000001F4C8F927AED3CA752257",
   prime    => "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF",
 }

=head1 FUNCTIONS

=head2 ecc_encrypt

Elliptic Curve Diffie-Hellman (ECDH) encryption as implemented by libtomcrypt. See method L</encrypt> below.

 my $ct = ecc_encrypt($pub_key_filename, $message);
 #or
 my $ct = ecc_encrypt(\$buffer_containing_pub_key, $message);
 #or
 my $ct = ecc_encrypt($pub_key_filename, $message, $hash_name);

 #NOTE: $hash_name can be 'SHA1' (DEFAULT), 'SHA256' or any other hash supported by Crypt::Digest

ECCDH Encryption is performed by producing a random key, hashing it, and XOR'ing the digest against the plaintext.

=head2 ecc_decrypt

Elliptic Curve Diffie-Hellman (ECDH) decryption as implemented by libtomcrypt. See method L</decrypt> below.

 my $pt = ecc_decrypt($priv_key_filename, $ciphertext);
 #or
 my $pt = ecc_decrypt(\$buffer_containing_priv_key, $ciphertext);

=head2 ecc_sign_message

Elliptic Curve Digital Signature Algorithm (ECDSA) - signature generation. See method L</sign_message> below.

 my $sig = ecc_sign_message($priv_key_filename, $message);
 #or
 my $sig = ecc_sign_message(\$buffer_containing_priv_key, $message);
 #or
 my $sig = ecc_sign_message($priv_key, $message, $hash_name);

=head2 ecc_verify_message

Elliptic Curve Digital Signature Algorithm (ECDSA) - signature verification. See method L</verify_message> below.

 ecc_verify_message($pub_key_filename, $signature, $message) or die "ERROR";
 #or
 ecc_verify_message(\$buffer_containing_pub_key, $signature, $message) or die "ERROR";
 #or
 ecc_verify_message($pub_key, $signature, $message, $hash_name) or die "ERROR";

=head2 ecc_sign_hash

Elliptic Curve Digital Signature Algorithm (ECDSA) - signature generation. See method L</sign_hash> below.

 my $sig = ecc_sign_hash($priv_key_filename, $message_hash);
 #or
 my $sig = ecc_sign_hash(\$buffer_containing_priv_key, $message_hash);

=head2 ecc_verify_hash

Elliptic Curve Digital Signature Algorithm (ECDSA) - signature verification. See method L</verify_hash> below.

 ecc_verify_hash($pub_key_filename, $signature, $message_hash) or die "ERROR";
 #or
 ecc_verify_hash(\$buffer_containing_pub_key, $signature, $message_hash) or die "ERROR";

=head2 ecc_shared_secret

Elliptic curve Diffie-Hellman (ECDH) - construct a Diffie-Hellman shared secret with a private and public ECC key. See method L</shared_secret> below.

 #on Alice side
 my $shared_secret = ecc_shared_secret('Alice_priv_ecc1.der', 'Bob_pub_ecc1.der');

 #on Bob side
 my $shared_secret = ecc_shared_secret('Bob_priv_ecc1.der', 'Alice_pub_ecc1.der');

=head1 OpenSSL interoperability

 ### let's have:
 # ECC private key in PEM format - eckey.priv.pem
 # ECC public key in PEM format  - eckey.pub.pem
 # data file to be signed - input.data

=head2 Sign by OpenSSL, verify by Crypt::PK::ECC

Create signature (from commandline):

 openssl dgst -sha1 -sign eckey.priv.pem -out input.sha1-ec.sig input.data

Verify signature (Perl code):

 use Crypt::PK::ECC;
 use Crypt::Digest 'digest_file';
 use Crypt::Misc 'read_rawfile';

 my $pkec = Crypt::PK::ECC->new("eckey.pub.pem");
 my $signature = read_rawfile("input.sha1-ec.sig");
 my $valid = $pkec->verify_hash($signature, digest_file("SHA1", "input.data"), "SHA1", "v1.5");
 print $valid ? "SUCCESS" : "FAILURE";

=head2 Sign by Crypt::PK::ECC, verify by OpenSSL

Create signature (Perl code):

 use Crypt::PK::ECC;
 use Crypt::Digest 'digest_file';
 use Crypt::Misc 'write_rawfile';

 my $pkec = Crypt::PK::ECC->new("eckey.priv.pem");
 my $signature = $pkec->sign_hash(digest_file("SHA1", "input.data"), "SHA1", "v1.5");
 write_rawfile("input.sha1-ec.sig", $signature);

Verify signature (from commandline):

 openssl dgst -sha1 -verify eckey.pub.pem -signature input.sha1-ec.sig input.data

=head2 Keys generated by Crypt::PK::ECC

Generate keys (Perl code):

 use Crypt::PK::ECC;
 use Crypt::Misc 'write_rawfile';

 my $pkec = Crypt::PK::ECC->new;
 $pkec->generate_key('secp160k1');
 write_rawfile("eckey.pub.der",  $pkec->export_key_der('public'));
 write_rawfile("eckey.priv.der", $pkec->export_key_der('private'));
 write_rawfile("eckey.pub.pem",  $pkec->export_key_pem('public'));
 write_rawfile("eckey.priv.pem", $pkec->export_key_pem('private'));
 write_rawfile("eckey-passwd.priv.pem", $pkec->export_key_pem('private', 'secret'));

Use keys by OpenSSL:

 openssl ec -in eckey.priv.der -text -inform der
 openssl ec -in eckey.priv.pem -text
 openssl ec -in eckey-passwd.priv.pem -text -inform pem -passin pass:secret
 openssl ec -in eckey.pub.der -pubin -text -inform der
 openssl ec -in eckey.pub.pem -pubin -text

=head2 Keys generated by OpenSSL

Generate keys:

 openssl ecparam -param_enc explicit -name prime192v3 -genkey -out eckey.priv.pem
 openssl ec -param_enc explicit -in eckey.priv.pem -out eckey.pub.pem -pubout
 openssl ec -param_enc explicit -in eckey.priv.pem -out eckey.priv.der -outform der
 openssl ec -param_enc explicit -in eckey.priv.pem -out eckey.pub.der -outform der -pubout
 openssl ec -param_enc explicit -in eckey.priv.pem -out eckey.privc.der -outform der -conv_form compressed
 openssl ec -param_enc explicit -in eckey.priv.pem -out eckey.pubc.der -outform der -pubout -conv_form compressed
 openssl ec -param_enc explicit -in eckey.priv.pem -passout pass:secret -des3 -out eckey-passwd.priv.pem

Load keys (Perl code):

 use Crypt::PK::ECC;

 my $pkec = Crypt::PK::ECC->new;
 $pkec->import_key("eckey.pub.der");
 $pkec->import_key("eckey.pubc.der");
 $pkec->import_key("eckey.priv.der");
 $pkec->import_key("eckey.privc.der");
 $pkec->import_key("eckey.pub.pem");
 $pkec->import_key("eckey.priv.pem");
 $pkec->import_key("eckey-passwd.priv.pem", "secret");

=head1 SEE ALSO

=over

=item * L<https://en.wikipedia.org/wiki/Elliptic_curve_cryptography|https://en.wikipedia.org/wiki/Elliptic_curve_cryptography>

=item * L<https://en.wikipedia.org/wiki/Elliptic_curve_Diffie%E2%80%93Hellman|https://en.wikipedia.org/wiki/Elliptic_curve_Diffie%E2%80%93Hellman>

=item * L<https://en.wikipedia.org/wiki/ECDSA|https://en.wikipedia.org/wiki/ECDSA>

=back

=cut
