package Crypt::Misc;

use strict;
use warnings;
our $VERSION = '0.063';

require Exporter; our @ISA = qw(Exporter); ### use Exporter 5.57 'import';
use Carp 'croak';
our %EXPORT_TAGS = ( all => [qw(encode_b64   decode_b64
                                encode_b64u  decode_b64u
                                encode_b58b  decode_b58b
                                encode_b58f  decode_b58f
                                encode_b58r  decode_b58r
                                encode_b58t  decode_b58t
                                encode_b58s  decode_b58s
                                encode_b32r  decode_b32r
                                encode_b32b  decode_b32b
                                encode_b32z  decode_b32z
                                encode_b32c  decode_b32c
                                pem_to_der   der_to_pem
                                read_rawfile write_rawfile
                                slow_eq is_v4uuid random_v4uuid
                                increment_octets_be increment_octets_le
                               )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp 'carp';
use CryptX;
use Crypt::Digest 'digest_data';
use Crypt::Mode::CBC;
use Crypt::Mode::CFB;
use Crypt::Mode::ECB;
use Crypt::Mode::OFB;
use Crypt::Cipher;
use Crypt::PRNG 'random_bytes';

sub _encode_b58 {
  my ($bytes, $alphabet) = @_;

  return '' if !defined $bytes || length($bytes) == 0;

  # handle leading zero-bytes
  my $base58 = '';
  if ($bytes =~ /^(\x00+)/) {
    $base58 = ('0' x length($1));
  }
  $base58 .= _bin_to_radix($bytes, 58);

  if (defined $alphabet) {
    my $default = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv";
    return undef if $alphabet !~ /^[a-zA-Z0-9]{58}$/;
    eval "\$base58 =~ tr/$default/$alphabet/"; # HACK: https://stackoverflow.com/questions/11415045/using-a-char-variable-in-tr
    return undef if $@;
  }

  return $base58;
}

sub _decode_b58 {
  my ($base58, $alphabet) = @_;

  return '' if !defined $base58 || length($base58) == 0;

  my $default = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv";
  if (defined $alphabet) {
    return undef if $alphabet !~ /^[a-zA-Z0-9]{58}$/ || $base58 !~ /^[$alphabet]+$/;
    eval "\$base58 =~ tr/$alphabet/$default/"; # HACK: https://stackoverflow.com/questions/11415045/using-a-char-variable-in-tr
    return undef if $@;
  }
  return undef if $base58 !~ /^[$default]+$/;

  # handle leading zeroes
  my $bytes = '';
  if ($base58 =~ /^(0+)(.*)$/) {
    $base58 = $2;
    $bytes = ("\x00" x length($1));
  }
  $bytes .= _radix_to_bin($base58, 58) if defined $base58 && length($base58) > 0;

  return $bytes;
}

sub decode_b58b { _decode_b58(shift, "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz") } # Bitcoin
sub decode_b58f { _decode_b58(shift, "123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ") } # Flickr
sub decode_b58r { _decode_b58(shift, "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz") } # Ripple
sub decode_b58t { _decode_b58(shift, "RPShNAF39wBUDnEGHJKLM4pQrsT7VWXYZ2bcdeCg65jkm8ofqi1tuvaxyz") } # Tipple
sub decode_b58s { _decode_b58(shift, "gsphnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCr65jkm8oFqi1tuvAxyz") } # Stellar

sub encode_b58b { _encode_b58(shift, "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz") } # Bitcoin
sub encode_b58f { _encode_b58(shift, "123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ") } # Flickr
sub encode_b58r { _encode_b58(shift, "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz") } # Ripple
sub encode_b58t { _encode_b58(shift, "RPShNAF39wBUDnEGHJKLM4pQrsT7VWXYZ2bcdeCg65jkm8ofqi1tuvaxyz") } # Tipple
sub encode_b58s { _encode_b58(shift, "gsphnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCr65jkm8oFqi1tuvAxyz") } # Stellar

sub pem_to_der {
  my ($data, $password) = @_;

  my ($begin, $obj1, $content, $end, $obj2);
  # first try to load KEY (e.g. EC pem files might contain more parts)
  ($begin, $obj1, $content, $end, $obj2) = $data =~ m/(----[- ]BEGIN ([^\r\n\-]+KEY)[ -]----)(.*?)(----[- ]END ([^\r\n\-]+)[ -]----)/s;
  # if failed then try to load anything
  ($begin, $obj1, $content, $end, $obj2) = $data =~ m/(----[- ]BEGIN ([^\r\n\-]+)[ -]----)(.*?)(----[- ]END ([^\r\n\-]+)[ -]----)/s unless $content;
  return undef unless $content;

  $content =~ s/^\s+//sg;
  $content =~ s/\s+$//sg;
  $content =~ s/\r\n/\n/sg;  # CR-LF >> LF
  $content =~ s/\r/\n/sg;    # CR >> LF
  $content =~ s/\\\n//sg;    # \ + LF

  my ($headers, undef, $b64) = $content =~ /^(([^:]+:.*?\n)*)(.*)$/s;
  return undef unless $b64;

  my $binary = decode_b64($b64);
  return undef unless $binary;

  my ($ptype, $cipher_name, $iv_hex);
  for my $h (split /\n/, ($headers||'')) {
    my ($k, $v) = split /:\s*/, $h, 2;
    $ptype = $v if $k eq 'Proc-Type';
    ($cipher_name, $iv_hex) = $v =~ /^\s*(.*?)\s*,\s*([0-9a-fA-F]+)\s*$/ if $k eq 'DEK-Info';
  }
  if ($cipher_name && $iv_hex && $ptype && $ptype eq '4,ENCRYPTED') {
    croak "FATAL: encrypted PEM but no password provided" unless defined $password;
    my $iv = pack("H*", $iv_hex);
    my ($mode, $klen) = _name2mode($cipher_name);
    my $key = _password2key($password, $klen, $iv, 'MD5');
    return $mode->decrypt($binary, $key, $iv);
  }
  return $binary;
}

sub der_to_pem {
  my ($data, $header_name, $password, $cipher_name) = @_;
  my $content = $data;
  my @headers;

  if ($password) {
    $cipher_name ||= 'AES-256-CBC';
    my ($mode, $klen, $ilen) = _name2mode($cipher_name);
    my $iv = random_bytes($ilen);
    my $key = _password2key($password, $klen, $iv, 'MD5');
    $content = $mode->encrypt($data, $key, $iv);
    push @headers, 'Proc-Type: 4,ENCRYPTED', "DEK-Info: ".uc($cipher_name).",".unpack("H*", $iv);
  }

  my $pem = "-----BEGIN $header_name-----\n";
  if (@headers) {
    $pem .= "$_\n" for @headers;
    $pem .= "\n";
  }
  my @l = encode_b64($content) =~ /.{1,64}/g;
  $pem .= join("\n", @l) . "\n";
  $pem .= "-----END $header_name-----\n";
  return $pem;
}

sub read_rawfile {
  # $data = read_rawfile($filename);
  my $f = shift;
  croak "FATAL: read_rawfile() non-existing file '$f'" unless -f $f;
  open my $fh, "<", $f or croak "FATAL: read_rawfile() cannot open file '$f': $!";
  binmode $fh;
  return do { local $/; <$fh> };
}

sub write_rawfile {
  # write_rawfile($filename, $data);
  croak "FATAL: write_rawfile() no data" unless defined $_[1];
  open my $fh, ">", $_[0] or croak "FATAL: write_rawfile() cannot open file '$_[0]': $!";
  binmode $fh;
  print $fh $_[1] or croak "FATAL: write_rawfile() cannot write to '$_[0]': $!";
  close $fh       or croak "FATAL: write_rawfile() cannot close '$_[0]': $!";
  return;
}

sub slow_eq {
  my ($a, $b) = @_;
  return unless defined $a && defined $b;
  my $diff = length $a ^ length $b;
  for(my $i = 0; $i < length $a && $i < length $b; $i++) {
    $diff |= ord(substr $a, $i) ^ ord(substr $b, $i);
  }
  return $diff == 0;
}

sub random_v4uuid() {
  # Version 4 - random - UUID: xxxxxxxx-xxxx-4xxx-Yxxx-xxxxxxxxxxxx
  # where x is any hexadecimal digit and Y is one of 8, 9, A, B (1000, 1001, 1010, 1011)
  # e.g. f47ac10b-58cc-4372-a567-0e02b2c3d479
  my $raw = random_bytes(16);
  #                   xxxxxxxxxxxx4xxxYxxxxxxxxxxxxxxx
  $raw &= pack("H*", "FFFFFFFFFFFF0FFFFFFFFFFFFFFFFFFF");
  $raw |= pack("H*", "00000000000040000000000000000000");
  $raw &= pack("H*", "FFFFFFFFFFFFFFFF3FFFFFFFFFFFFFFF"); # 0x3 == 0011b
  $raw |= pack("H*", "00000000000000008000000000000000"); # 0x8 == 1000b
  my $hex = unpack("H*", $raw);
  $hex =~ s/^(.{8})(.{4})(.{4})(.{4})(.{12}).*$/$1-$2-$3-$4-$5/;
  return $hex;
}

sub is_v4uuid($) {
  my $uuid = shift;
  return 0 if !$uuid;
  return 1 if $uuid =~ /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
  return 0;
}

###  private functions

sub _name2mode {
  my $cipher_name = uc(shift);
  my %trans = ( 'DES-EDE3' => 'DES_EDE' );

  my ($cipher, undef, $klen, $mode) = $cipher_name =~ /^(AES|CAMELLIA|DES|DES-EDE3|SEED)(-(\d+))?-(CBC|CFB|ECB|OFB)$/i;
  croak "FATAL: unsupported cipher '$cipher_name'" unless $cipher && $mode;
  $cipher = $trans{$cipher} || $cipher;
  $klen = 192 if $cipher eq 'DES_EDE';
  $klen = 64  if $cipher eq 'DES';
  $klen = 128 if $cipher eq 'SEED';
  $klen = $klen ? int($klen/8) : Crypt::Cipher::min_keysize($cipher);
  my $ilen = Crypt::Cipher::blocksize($cipher);
  croak "FATAL: unsupported cipher '$cipher_name'" unless $klen && $ilen;

  return (Crypt::Mode::CBC->new($cipher), $klen, $ilen) if $mode eq 'CBC';
  return (Crypt::Mode::CFB->new($cipher), $klen, $ilen) if $mode eq 'CFB';
  return (Crypt::Mode::ECB->new($cipher), $klen, $ilen) if $mode eq 'ECB';
  return (Crypt::Mode::OFB->new($cipher), $klen, $ilen) if $mode eq 'OFB';
}

sub _password2key {
  my ($password, $klen, $iv, $hash) = @_;
  my $salt = substr($iv, 0, 8);
  my $key = '';
  while (length($key) < $klen) {
    $key .= digest_data($hash, $key . $password . $salt);
  }
  return substr($key, 0, $klen);
}

1;

=pod

=head1 NAME

Crypt::Misc - miscellaneous functions related to (or used by) CryptX

=head1 SYNOPSIS

This module contains a collection of mostly unsorted functions loosely-related to CryptX distribution but not implementing cryptography.

Most of them are also available in other perl modules but once you utilize CryptX you might avoid dependencies on other modules by using
functions from Crypt::Misc.

=head1 DESCRIPTION

 use Crypt::Misc ':all';

 # Base64 and Base64/URL-safe functions
 $base64    = encode_b64($rawbytes);
 $rawbytes  = decode_b64($base64);
 $base64url = encode_b64u($encode_b64u);
 $rawbytes  = decode_b64u($base64url);

 # read/write file
 $rawdata = read_rawfile($filename);
 write_rawfile($filename, $rawdata);

 # convert PEM/DER
 $der_data = pem_to_der($pem_data);
 $pem_data = der_to_pem($der_data);

  # others
  die "mismatch" unless slow_eq($str1, $str2);

=head1 FUNCTIONS

By default, Crypt::Misc doesn't import any function. You can import individual functions like this:

 use Crypt::Misc qw(read_rawfile);

Or import all available functions:

 use Crypt::Misc ':all';

=head2  read_rawfile

I<Since: 0.029>

 $rawdata = read_rawfile($filename);

Read file C<$filename> into a scalar as a binary data (without decoding/transformation).

=head2  write_rawfile

I<Since: 0.029>

 write_rawfile($filename, $rawdata);

Write C<$rawdata> to file C<$filename> as binary data.

=head2  slow_eq

I<Since: 0.029>

 if (slow_eq($data1, $data2)) { ... }

Constant time compare (to avoid timing side-channel).

=head2  pem_to_der

I<Since: 0.029>

  $der_data = pem_to_der($pem_data);
  #or
  $der_data = pem_to_der($pem_data, $password);

Convert PEM to DER representation. Supports also password protected PEM data.

=head2  der_to_pem

I<Since: 0.029>

  $pem_data = der_to_pem($der_data, $header_name);
  #or
  $pem_data = der_to_pem($der_data, $header_name, $password);
  #or
  $pem_data = der_to_pem($der_data, $header_name, $passord, $cipher_name);

  # $header_name e.g. "PUBLIC KEY", "RSA PRIVATE KEY" ...
  # $cipher_name e.g. "DES-EDE3-CBC", "AES-256-CBC" (DEFAULT) ...

Convert DER to PEM representation. Supports also password protected PEM data.

=head2  random_v4uuid

I<Since: 0.031>

 my $uuid = random_v4uuid();

Returns cryptographically strong Version 4 random UUID: C<xxxxxxxx-xxxx-4xxx-Yxxx-xxxxxxxxxxxx>
where C<x> is any hexadecimal digit and C<Y> is one of 8, 9, A, B (1000, 1001, 1010, 1011)
e.g. C<f47ac10b-58cc-4372-a567-0e02b2c3d479>.

=head2  is_v4uuid

I<Since: 0.031>

  if (is_v4uuid($uuid)) {
    ...
  }

Checks the given C<$uuid> string whether it matches V4 UUID format and returns C<0> (mismatch) or C<1> (match).

=head2 increment_octets_le

I<Since: 0.048>

 $octects = increment_octets_le($octets);

Take input C<$octets> as a little-endian big number and return an increment.

=head2 increment_octets_be

I<Since: 0.048>

 $octects = increment_octets_be($octets);

Take input C<$octets> as a big-endian big number and return an increment.

=head2 encode_b64

I<Since: 0.029>

 $base64string = encode_b64($rawdata);

Encode $rawbytes into Base64 string, no line-endings in the output string.

=head2 decode_b64

I<Since: 0.029>

 $rawdata = decode_b64($base64string);

Decode a Base64 string.

=head2  encode_b64u

I<Since: 0.029>

 $base64url_string = encode_b64($rawdata);

Encode $rawbytes into Base64/URL-Safe string, no line-endings in the output string.

=head2  decode_b64u

I<Since: 0.029>

 $rawdata = decode_b64($base64url_string);

Decode a Base64/URL-Safe string.

=head2  encode_b32r

I<Since: 0.049>

 $string = encode_b32r($rawdata);

Encode bytes into Base32 (rfc4648 alphabet) string, without "=" padding.

=head2  decode_b32r

I<Since: 0.049>

 $rawdata = decode_b32r($string);

Decode a Base32 (rfc4648 alphabet) string into bytes.

=head2  encode_b32b

I<Since: 0.049>

 $string = encode_b32b($rawdata);

Encode bytes into Base32 (base32hex alphabet) string, without "=" padding.

=head2  decode_b32b

I<Since: 0.049>

 $rawdata = decode_b32b($string);

Decode a Base32 (base32hex alphabet) string into bytes.

=head2  encode_b32z

I<Since: 0.049>

 $string = encode_b32z($rawdata);

Encode bytes into Base32 (zbase32 alphabet) string.

=head2  decode_b32z

I<Since: 0.049>

 $rawdata = decode_b32z($string);

Decode a Base32 (zbase32 alphabet) string into bytes.

=head2  encode_b32c

I<Since: 0.049>

 $string = encode_b32c($rawdata);

Encode bytes into Base32 (crockford alphabet) string.

=head2  decode_b32c

I<Since: 0.049>

 $rawdata = decode_b32c($string);

Decode a Base32 (crockford alphabet) string into bytes.

=head2  encode_b58b

I<Since: 0.049>

 $string = encode_b58b($rawdata);

Encode bytes into Base58 (Bitcoin alphabet) string.

=head2  decode_b58b

I<Since: 0.049>

 $rawdata = decode_b58b($string);

Decode a Base58 (Bitcoin alphabet) string into bytes.

=head2  encode_b58f

I<Since: 0.049>

 $string = encode_b58f($rawdata);

Encode bytes into Base58 (Flickr alphabet) string.

=head2  decode_b58f

I<Since: 0.049>

 $rawdata = decode_b58f($string);

Decode a Base58 (Flickr alphabet) string into bytes.

=head2  encode_b58r

I<Since: 0.049>

 $string = encode_b58r($rawdata);

Encode bytes into Base58 (Ripple alphabet) string.

=head2  decode_b58r

I<Since: 0.049>

 $rawdata = decode_b58r($string);

Decode a Base58 (Ripple alphabet) string into bytes.

=head2  encode_b58t

I<Since: 0.049>

 $string = encode_b58t($rawdata);

Encode bytes into Base58 (Tipple alphabet) string.

=head2  decode_b58t

I<Since: 0.049>

 $rawdata = decode_b58t($string);

Decode a Base58 (Tipple alphabet) string into bytes.

=head2  encode_b58s

I<Since: 0.049>

 $string = encode_b58s($rawdata);

Encode bytes into Base58 (Stellar alphabet) string.

=head2  decode_b58s

I<Since: 0.049>

 $rawdata = decode_b58s($string);

Decode a Base58 (Stellar alphabet) string into bytes.

=head1 SEE ALSO

=over

=item * L<CryptX|CryptX>

=back

=cut
