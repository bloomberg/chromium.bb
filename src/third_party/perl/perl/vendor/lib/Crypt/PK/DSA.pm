package Crypt::PK::DSA;

use strict;
use warnings;
our $VERSION = '0.063';

require Exporter; our @ISA = qw(Exporter); ### use Exporter 'import';
our %EXPORT_TAGS = ( all => [qw( dsa_encrypt dsa_decrypt dsa_sign_message dsa_verify_message dsa_sign_hash dsa_verify_hash )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
$Carp::Internal{(__PACKAGE__)}++;
use CryptX;
use Crypt::Digest 'digest_data';
use Crypt::Misc qw(read_rawfile encode_b64u decode_b64u encode_b64 decode_b64 pem_to_der der_to_pem);
use Crypt::PK;

sub new {
  my $self = shift->_new();
  return @_ > 0 ? $self->import_key(@_) : $self;
}

sub generate_key {
  my $self = shift;
  return $self->_generate_key_size(@_) if @_ == 2;
  if (@_ == 1 && ref $_[0] eq 'HASH') {
    my $param = shift;
    my $p = $param->{p} or croak "FATAL: 'p' param not specified";
    my $q = $param->{q} or croak "FATAL: 'q' param not specified";
    my $g = $param->{g} or croak "FATAL: 'g' param not specified";
    $p =~ s/^0x//;
    $q =~ s/^0x//;
    $g =~ s/^0x//;
    return $self->_generate_key_pqg_hex($p, $q, $g);
  }
  elsif (@_ == 1 && ref $_[0] eq 'SCALAR') {
    my $data = ${$_[0]};
    $data = pem_to_der($data) if $data =~ /-----BEGIN DSA PARAMETERS-----\s*(.+)\s*-----END DSA PARAMETERS-----/s;
    return $self->_generate_key_dsaparam($data);
  }
  croak "FATAL: DSA generate_key - invalid args";
}

sub export_key_pem {
  my ($self, $type, $password, $cipher) = @_;
  my $key = $self->export_key_der($type||'');
  return unless $key;
  return der_to_pem($key, "DSA PRIVATE KEY", $password, $cipher) if $type eq 'private';
  return der_to_pem($key, "DSA PUBLIC KEY") if $type eq 'public';
  return der_to_pem($key, "PUBLIC KEY") if $type eq 'public_x509';
}

sub import_key {
  my ($self, $key, $password) = @_;
  croak "FATAL: undefined key" unless $key;

  # special case
  if (ref($key) eq 'HASH') {
    if ($key->{p} && $key->{q} && $key->{g} && $key->{y}) {
      # hash exported via key2hash
      return $self->_import_hex($key->{p}, $key->{q}, $key->{g}, $key->{x}, $key->{y});
    }
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

  if ($data =~ /-----BEGIN (DSA PRIVATE|DSA PUBLIC|PRIVATE|PUBLIC) KEY-----(.*?)-----END/sg) {
    $data = pem_to_der($data, $password);
    return $self->_import($data);
  }
  elsif ($data =~ /---- BEGIN SSH2 PUBLIC KEY ----(.*?)---- END SSH2 PUBLIC KEY ----/sg) {
    $data = pem_to_der($data);
    my ($typ, $p, $q, $g, $y) = Crypt::PK::_ssh_parse($data);
    return $self->_import_hex(unpack('H*',$p), unpack('H*',$q), unpack('H*',$g), undef, unpack('H*',$y)) if $typ && $p && $q && $g && $y && $typ eq 'ssh-dss';
  }
  elsif ($data =~ /ssh-dss\s+(\S+)/) {
    $data = decode_b64("$1");
    my ($typ, $p, $q, $g, $y) = Crypt::PK::_ssh_parse($data);
    return $self->_import_hex(unpack('H*',$p), unpack('H*',$q), unpack('H*',$g), undef, unpack('H*',$y)) if $typ && $p && $q && $g && $y && $typ eq 'ssh-dss';
  }
  else {
    return $self->_import($data);
  }
  croak "FATAL: invalid or unsupported DSA key format";
}

### FUNCTIONS

sub dsa_encrypt {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->encrypt(@_);
}

sub dsa_decrypt {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->decrypt(@_);
}

sub dsa_sign_message {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->sign_message(@_);
}

sub dsa_verify_message {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->verify_message(@_);
}

sub dsa_sign_hash {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->sign_hash(@_);
}

sub dsa_verify_hash {
  my $key = shift;
  local $SIG{__DIE__} = \&CryptX::_croak;
  $key = __PACKAGE__->new($key) unless ref $key;
  carp "FATAL: invalid 'key' param" unless ref($key) eq __PACKAGE__;
  return $key->verify_hash(@_);
}

sub CLONE_SKIP { 1 } # prevent cloning

1;

=pod

=head1 NAME

Crypt::PK::DSA - Public key cryptography based on DSA

=head1 SYNOPSIS

 ### OO interface

 #Encryption: Alice
 my $pub = Crypt::PK::DSA->new('Bob_pub_dsa1.der');
 my $ct = $pub->encrypt("secret message");
 #
 #Encryption: Bob (received ciphertext $ct)
 my $priv = Crypt::PK::DSA->new('Bob_priv_dsa1.der');
 my $pt = $priv->decrypt($ct);

 #Signature: Alice
 my $priv = Crypt::PK::DSA->new('Alice_priv_dsa1.der');
 my $sig = $priv->sign_message($message);
 #
 #Signature: Bob (received $message + $sig)
 my $pub = Crypt::PK::DSA->new('Alice_pub_dsa1.der');
 $pub->verify_message($sig, $message) or die "ERROR";

 #Key generation
 my $pk = Crypt::PK::DSA->new();
 $pk->generate_key(30, 256);
 my $private_der = $pk->export_key_der('private');
 my $public_der = $pk->export_key_der('public');
 my $private_pem = $pk->export_key_pem('private');
 my $public_pem = $pk->export_key_pem('public');

 ### Functional interface

 #Encryption: Alice
 my $ct = dsa_encrypt('Bob_pub_dsa1.der', "secret message");
 #Encryption: Bob (received ciphertext $ct)
 my $pt = dsa_decrypt('Bob_priv_dsa1.der', $ct);

 #Signature: Alice
 my $sig = dsa_sign_message('Alice_priv_dsa1.der', $message);
 #Signature: Bob (received $message + $sig)
 dsa_verify_message('Alice_pub_dsa1.der', $sig, $message) or die "ERROR";

=head1 METHODS

=head2 new

  my $pk = Crypt::PK::DSA->new();
  #or
  my $pk = Crypt::PK::DSA->new($priv_or_pub_key_filename);
  #or
  my $pk = Crypt::PK::DSA->new(\$buffer_containing_priv_or_pub_key);

Support for password protected PEM keys

  my $pk = Crypt::PK::DSA->new($priv_pem_key_filename, $password);
  #or
  my $pk = Crypt::PK::DSA->new(\$buffer_containing_priv_pem_key, $password);

=head2 generate_key

Uses Yarrow-based cryptographically strong random number generator seeded with
random data taken from C</dev/random> (UNIX) or C<CryptGenRandom> (Win32).

 $pk->generate_key($group_size, $modulus_size);
 # $group_size  ... in bytes .. 15 < $group_size < 1024
 # $modulus_size .. in bytes .. ($modulus_size - $group_size) < 512

 ### Bits of Security according to libtomcrypt documentation
 # 80 bits   => generate_key(20, 128)
 # 120 bits  => generate_key(30, 256)
 # 140 bits  => generate_key(35, 384)
 # 160 bits  => generate_key(40, 512)

 ### Sizes according section 4.2 of FIPS 186-4
 # (L and N are the bit lengths of p and q respectively)
 # L = 1024, N = 160 => generate_key(20, 128)
 # L = 2048, N = 224 => generate_key(28, 256)
 # L = 2048, N = 256 => generate_key(32, 256)
 # L = 3072, N = 256 => generate_key(32, 384)

 $pk->generate_key($param_hash)
 # $param_hash is { d => $d, p => $p, q => $q }
 # where $d, $p, $q are hex strings

 $pk->generate_key(\$dsa_param)
 # $dsa_param is the content of DER or PEM file with DSA params
 # e.g. openssl dsaparam 2048

=head2 import_key

Loads private or public key in DER or PEM format.

  $pk->import_key($filename);
  #or
  $pk->import_key(\$buffer_containing_key);

Support for password protected PEM keys

  $pk->import_key($pem_filename, $password);
  #or
  $pk->import_key(\$buffer_containing_pem_key, $password);

Loading private or public keys form perl hash:

 $pk->import_key($hashref);

 # where $hashref is a key exported via key2hash
 $pk->import_key({
   p => "AAF839A764E04D80824B79FA1F0496C093...", #prime modulus
   q => "D05C4CB45F29D353442F1FEC43A6BE2BE8...", #prime divisor
   g => "847E8896D12C9BF18FE283AE7AD58ED7F3...", #generator of a subgroup of order q in GF(p)
   x => "6C801901AC74E2DC714D75A9F6969483CF...", #private key, random  0 < x < q
   y => "8F7604D77FA62C7539562458A63C7611B7...", #public key, where y = g^x mod p
 });

Supported key formats:

=over

=item * DSA public keys

 -----BEGIN PUBLIC KEY-----
 MIIBtjCCASsGByqGSM44BAEwggEeAoGBAJKyu+puNMGLpGIhbD1IatnwlI79ePr4
 YHe2KBhRkheKxWUZRpN1Vd/+usS2IHSJ9op5cSWETiP05d7PMtJaitklw7jhudq3
 GxNvV/GRdCQm3H6d76FHP88dms4vcDYc6ry6wKERGfNEtZ+4BAKrMZK+gDYsF4Aw
 U6WVR969kYZhAhUA6w25FgSRmJ8W4XkvC60n8Wv3DpMCgYA4ZFE+3tLOM24PZj9Z
 rxuqUzZZdR+kIzrsIYpWN9ustbmdKLKwsqIaUIxc5zxHEhbAjAIf8toPD+VEQIpY
 7vgJgDhXuPq45BgN19iLTzOJwIhAFXPZvnAdIo9D/AnMw688gT6g6U8QCZwX2XYg
 ICiVcriYVNcjVKHSFY/X0Oi7CgOBhAACgYB4ZTn4OYT/pjUd6tNhGPtOS3CE1oaj
 5ScbetXg4ZDpceEyQi8VG+/ZTbs8var8X77JdEdeQA686cAxpOaVgW8V4odvcmfA
 BfueiGnPXjqGfppiHAyL1Ngyd+EsXKmKVXZYAVFVI0WuJKiZBSVURU7+ByxOfpGa
 fZhibr0SggWixQ==
 -----END PUBLIC KEY-----

=item * DSA private keys

 -----BEGIN DSA PRIVATE KEY-----
 MIIBuwIBAAKBgQCSsrvqbjTBi6RiIWw9SGrZ8JSO/Xj6+GB3tigYUZIXisVlGUaT
 dVXf/rrEtiB0ifaKeXElhE4j9OXezzLSWorZJcO44bnatxsTb1fxkXQkJtx+ne+h
 Rz/PHZrOL3A2HOq8usChERnzRLWfuAQCqzGSvoA2LBeAMFOllUfevZGGYQIVAOsN
 uRYEkZifFuF5LwutJ/Fr9w6TAoGAOGRRPt7SzjNuD2Y/Wa8bqlM2WXUfpCM67CGK
 VjfbrLW5nSiysLKiGlCMXOc8RxIWwIwCH/LaDw/lRECKWO74CYA4V7j6uOQYDdfY
 i08zicCIQBVz2b5wHSKPQ/wJzMOvPIE+oOlPEAmcF9l2ICAolXK4mFTXI1Sh0hWP
 19DouwoCgYB4ZTn4OYT/pjUd6tNhGPtOS3CE1oaj5ScbetXg4ZDpceEyQi8VG+/Z
 Tbs8var8X77JdEdeQA686cAxpOaVgW8V4odvcmfABfueiGnPXjqGfppiHAyL1Ngy
 d+EsXKmKVXZYAVFVI0WuJKiZBSVURU7+ByxOfpGafZhibr0SggWixQIVAL7Sia03
 8bvANjjL9Sitk8slrM6P
 -----END DSA PRIVATE KEY-----

=item * DSA private keys in password protected PEM format:

 -----BEGIN DSA PRIVATE KEY-----
 Proc-Type: 4,ENCRYPTED
 DEK-Info: DES-CBC,227ADC3AA0299491

 UISxBYAxPQMl2eK9LMAeHsssF6IxO+4G2ta2Jn8VE+boJrrH3iSTKeMXGjGaXl0z
 DwcLGV+KMR70y+cxtTb34rFy+uSpBy10dOQJhxALDbe1XfCDQIUfaXRfMNA3um2I
 JdZixUD/zcxBOUzao+MCr0V9XlJDgqBhJ5EEr53XHH07Eo5fhiBfbbR9NzdUPFrQ
 p2ASyZtFh7RXoIBUCQgg21oeLddcNWV7gd/Y46kghO9s0JbJ8C+IsuWEPRSq502h
 tSoDN6B0sxbVvOUICLLbQaxt7yduTAhRxVIJZ1PWATTVD7CZBVz9uIDZ7LOv+er2
 1q3vkwb8E9spPsA240+BnfD571XEop4jrawxC0VKQZ+3cPVLc6jhIsxvzzFQUt67
 g66v8GUgt7KF3KhVV7qEtntybQWDWb+K/uTIH9Ra8nP820d3Rnl61pPXDPlluteT
 WSLOvEMN2zRmkaxQNv/tLdT0SYpQtdjw74G3A6T7+KnvinKrjtp1a/AXkCF9hNEx
 DGbxOYo1UOmk8qdxWCrab34nO+Q8oQc9wjXHG+ZtRYIMoGMKREK8DeL4H1RPNkMf
 rwXWk8scd8QFmJAb8De1VQ==
 -----END DSA PRIVATE KEY-----

=item * SSH public DSA keys

 ssh-dss AAAAB3NzaC1kc3MAAACBAKU8/avmk...4XOwuEssAVhmwA==

=item * SSH public DSA keys (RFC-4716 format)

 ---- BEGIN SSH2 PUBLIC KEY ----
 Comment: "1024-bit DSA, converted from OpenSSH"
 AAAAB3NzaC1kc3MAAACBAKU8/avmkFeGnSqwYG7dZnQlG+01QNaxu3F5v0NcL/SRUW7Idp
 Uq8t14siK0mA6yjphLhOf5t8gugTEVBllP86ANSbFigH7WN3v6ydJWqm60pNhNHN//50cn
 NtIsXbxeq3VtsI64pkH1OJqeZDHLmu73k4T0EKOzsylSfF/wtVBJAAAAFQChpubLHViwPB
 +jSvUb8e4THS7PBQAAAIAJD1PMCiTCQa1xyD/NCWOajCufTOIzKAhm6l+nlBVPiKI+262X
 pYt127Ke4mPL8XJBizoTjSQN08uHMg/8L6W/cdO2aZ+mhkBnS1xAm83DAwqLrDraR1w/4Q
 RFxr5Vbyy8qnejrPjTJobBN1BGsv84wHkjmoCn6pFIfkGYeATlJgAAAIAHYPU1zMVBTDWr
 u7SNC4G2UyWGWYYLjLytBVHfQmBa51CmqrSs2kCfGLGA1ynfYENsxcJq9nsXrb4i17H5BH
 JFkH0g7BUDpeBeLr8gsK3WgfqWwtZsDkltObw9chUD/siK6q/dk/fSIB2Ho0inev7k68Z5
 ZkNI4XOwuEssAVhmwA==
 ---- END SSH2 PUBLIC KEY ----

=back

=head2 export_key_der

 my $private_der = $pk->export_key_der('private');
 #or
 my $public_der = $pk->export_key_der('public');

=head2 export_key_pem

 my $private_pem = $pk->export_key_pem('private');
 #or
 my $public_pem = $pk->export_key_pem('public');
 #or
 my $public_pem = $pk->export_key_pem('public_x509');

With parameter C<'public'> uses header and footer lines:

  -----BEGIN DSA PUBLIC KEY------
  -----END DSA PUBLIC KEY------

With parameter C<'public_x509'> uses header and footer lines:

  -----BEGIN PUBLIC KEY------
  -----END PUBLIC KEY------

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

=head2 encrypt

 my $pk = Crypt::PK::DSA->new($pub_key_filename);
 my $ct = $pk->encrypt($message);
 #or
 my $ct = $pk->encrypt($message, $hash_name);

 #NOTE: $hash_name can be 'SHA1' (DEFAULT), 'SHA256' or any other hash supported by Crypt::Digest

=head2 decrypt

 my $pk = Crypt::PK::DSA->new($priv_key_filename);
 my $pt = $pk->decrypt($ciphertext);

=head2 sign_message

 my $pk = Crypt::PK::DSA->new($priv_key_filename);
 my $signature = $priv->sign_message($message);
 #or
 my $signature = $priv->sign_message($message, $hash_name);

 #NOTE: $hash_name can be 'SHA1' (DEFAULT), 'SHA256' or any other hash supported by Crypt::Digest

=head2 verify_message

 my $pk = Crypt::PK::DSA->new($pub_key_filename);
 my $valid = $pub->verify_message($signature, $message)
 #or
 my $valid = $pub->verify_message($signature, $message, $hash_name);

 #NOTE: $hash_name can be 'SHA1' (DEFAULT), 'SHA256' or any other hash supported by Crypt::Digest

=head2 sign_hash

 my $pk = Crypt::PK::DSA->new($priv_key_filename);
 my $signature = $priv->sign_hash($message_hash);

=head2 verify_hash

 my $pk = Crypt::PK::DSA->new($pub_key_filename);
 my $valid = $pub->verify_hash($signature, $message_hash);

=head2 is_private

 my $rv = $pk->is_private;
 # 1 .. private key loaded
 # 0 .. public key loaded
 # undef .. no key loaded

=head2 size

 my $size = $pk->size;
 # returns key size (length of the prime p) in bytes or undef if key not loaded

=head2 size_q

 my $size = $pk->size_q;
 # returns length of the prime q in bytes or undef if key not loaded

=head2 key2hash

 my $hash = $pk->key2hash;

 # returns hash like this (or undef if no key loaded):
 {
   type => 1,   # integer: 1 .. private, 0 .. public
   size => 256, # integer: key size in bytes
   # all the rest are hex strings
   p => "AAF839A764E04D80824B79FA1F0496C093...", #prime modulus
   q => "D05C4CB45F29D353442F1FEC43A6BE2BE8...", #prime divisor
   g => "847E8896D12C9BF18FE283AE7AD58ED7F3...", #generator of a subgroup of order q in GF(p)
   x => "6C801901AC74E2DC714D75A9F6969483CF...", #private key, random  0 < x < q
   y => "8F7604D77FA62C7539562458A63C7611B7...", #public key, where y = g^x mod p
 }

=head1 FUNCTIONS

=head2 dsa_encrypt

DSA based encryption as implemented by libtomcrypt. See method L</encrypt> below.

 my $ct = dsa_encrypt($pub_key_filename, $message);
 #or
 my $ct = dsa_encrypt(\$buffer_containing_pub_key, $message);
 #or
 my $ct = dsa_encrypt($pub_key_filename, $message, $hash_name);

 #NOTE: $hash_name can be 'SHA1' (DEFAULT), 'SHA256' or any other hash supported by Crypt::Digest

Encryption works similar to the L<Crypt::PK::ECC> encryption whereas shared DSA key is computed, and
the hash of the shared key XOR'ed against the plaintext forms the ciphertext.

=head2 dsa_decrypt

DSA based decryption as implemented by libtomcrypt. See method L</decrypt> below.

 my $pt = dsa_decrypt($priv_key_filename, $ciphertext);
 #or
 my $pt = dsa_decrypt(\$buffer_containing_priv_key, $ciphertext);

=head2 dsa_sign_message

Generate DSA signature. See method L</sign_message> below.

 my $sig = dsa_sign_message($priv_key_filename, $message);
 #or
 my $sig = dsa_sign_message(\$buffer_containing_priv_key, $message);
 #or
 my $sig = dsa_sign_message($priv_key, $message, $hash_name);

=head2 dsa_verify_message

Verify DSA signature. See method L</verify_message> below.

 dsa_verify_message($pub_key_filename, $signature, $message) or die "ERROR";
 #or
 dsa_verify_message(\$buffer_containing_pub_key, $signature, $message) or die "ERROR";
 #or
 dsa_verify_message($pub_key, $signature, $message, $hash_name) or die "ERROR";

=head2 dsa_sign_hash

Generate DSA signature. See method L</sign_hash> below.

 my $sig = dsa_sign_hash($priv_key_filename, $message_hash);
 #or
 my $sig = dsa_sign_hash(\$buffer_containing_priv_key, $message_hash);

=head2 dsa_verify_hash

Verify DSA signature. See method L</verify_hash> below.

 dsa_verify_hash($pub_key_filename, $signature, $message_hash) or die "ERROR";
 #or
 dsa_verify_hash(\$buffer_containing_pub_key, $signature, $message_hash) or die "ERROR";

=head1 OpenSSL interoperability

 ### let's have:
 # DSA private key in PEM format - dsakey.priv.pem
 # DSA public key in PEM format  - dsakey.pub.pem
 # data file to be signed - input.data

=head2 Sign by OpenSSL, verify by Crypt::PK::DSA

Create signature (from commandline):

 openssl dgst -sha1 -sign dsakey.priv.pem -out input.sha1-dsa.sig input.data

Verify signature (Perl code):

 use Crypt::PK::DSA;
 use Crypt::Digest 'digest_file';
 use Crypt::Misc 'read_rawfile';

 my $pkdsa = Crypt::PK::DSA->new("dsakey.pub.pem");
 my $signature = read_rawfile("input.sha1-dsa.sig");
 my $valid = $pkdsa->verify_hash($signature, digest_file("SHA1", "input.data"), "SHA1", "v1.5");
 print $valid ? "SUCCESS" : "FAILURE";

=head2 Sign by Crypt::PK::DSA, verify by OpenSSL

Create signature (Perl code):

 use Crypt::PK::DSA;
 use Crypt::Digest 'digest_file';
 use Crypt::Misc 'write_rawfile';

 my $pkdsa = Crypt::PK::DSA->new("dsakey.priv.pem");
 my $signature = $pkdsa->sign_hash(digest_file("SHA1", "input.data"), "SHA1", "v1.5");
 write_rawfile("input.sha1-dsa.sig", $signature);

Verify signature (from commandline):

 openssl dgst -sha1 -verify dsakey.pub.pem -signature input.sha1-dsa.sig input.data

=head2 Keys generated by Crypt::PK::DSA

Generate keys (Perl code):

 use Crypt::PK::DSA;
 use Crypt::Misc 'write_rawfile';

 my $pkdsa = Crypt::PK::DSA->new;
 $pkdsa->generate_key(20, 128);
 write_rawfile("dsakey.pub.der",  $pkdsa->export_key_der('public'));
 write_rawfile("dsakey.priv.der", $pkdsa->export_key_der('private'));
 write_rawfile("dsakey.pub.pem",  $pkdsa->export_key_pem('public_x509'));
 write_rawfile("dsakey.priv.pem", $pkdsa->export_key_pem('private'));
 write_rawfile("dsakey-passwd.priv.pem", $pkdsa->export_key_pem('private', 'secret'));

Use keys by OpenSSL:

 openssl dsa -in dsakey.priv.der -text -inform der
 openssl dsa -in dsakey.priv.pem -text
 openssl dsa -in dsakey-passwd.priv.pem -text -inform pem -passin pass:secret
 openssl dsa -in dsakey.pub.der -pubin -text -inform der
 openssl dsa -in dsakey.pub.pem -pubin -text

=head2 Keys generated by OpenSSL

Generate keys:

 openssl dsaparam -genkey -out dsakey.priv.pem 1024
 openssl dsa -in dsakey.priv.pem -out dsakey.priv.der -outform der
 openssl dsa -in dsakey.priv.pem -out dsakey.pub.pem -pubout
 openssl dsa -in dsakey.priv.pem -out dsakey.pub.der -outform der -pubout
 openssl dsa -in dsakey.priv.pem -passout pass:secret -des3 -out dsakey-passwd.priv.pem

Load keys (Perl code):

 use Crypt::PK::DSA;

 my $pkdsa = Crypt::PK::DSA->new;
 $pkdsa->import_key("dsakey.pub.der");
 $pkdsa->import_key("dsakey.priv.der");
 $pkdsa->import_key("dsakey.pub.pem");
 $pkdsa->import_key("dsakey.priv.pem");
 $pkdsa->import_key("dsakey-passwd.priv.pem", "secret");

=head1 SEE ALSO

=over

=item * L<https://en.wikipedia.org/wiki/Digital_Signature_Algorithm|https://en.wikipedia.org/wiki/Digital_Signature_Algorithm>

=back

=cut
