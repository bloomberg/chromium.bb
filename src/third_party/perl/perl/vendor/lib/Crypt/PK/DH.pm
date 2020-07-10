package Crypt::PK::DH;

use strict;
use warnings;
our $VERSION = '0.063';

require Exporter; our @ISA = qw(Exporter); ### use Exporter 'import';
our %EXPORT_TAGS = ( all => [qw( dh_shared_secret )] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT = qw();

use Carp;
use CryptX;
use Crypt::Digest 'digest_data';
use Crypt::Misc qw(read_rawfile pem_to_der);

my %DH_PARAMS = (
  ike768  => { g => 2, p => 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1'.
                            '29024E088A67CC74020BBEA63B139B22514A08798E3404DD'.
                            'EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245'.
                            'E485B576625E7EC6F44C42E9A63A3620FFFFFFFFFFFFFFFF'
  },
  ike1024 => { g => 2, p => 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1'.
                            '29024E088A67CC74020BBEA63B139B22514A08798E3404DD'.
                            'EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245'.
                            'E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED'.
                            'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381'.
                            'FFFFFFFFFFFFFFFF'
  },
  ike1536 => { g => 2, p => 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1'.
                            '29024E088A67CC74020BBEA63B139B22514A08798E3404DD'.
                            'EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245'.
                            'E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED'.
                            'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D'.
                            'C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F'.
                            '83655D23DCA3AD961C62F356208552BB9ED529077096966D'.
                            '670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF'
  },
  ike2048 => { g => 2, p => 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1'.
                            '29024E088A67CC74020BBEA63B139B22514A08798E3404DD'.
                            'EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245'.
                            'E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED'.
                            'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D'.
                            'C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F'.
                            '83655D23DCA3AD961C62F356208552BB9ED529077096966D'.
                            '670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B'.
                            'E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9'.
                            'DE2BCBF6955817183995497CEA956AE515D2261898FA0510'.
                            '15728E5A8AACAA68FFFFFFFFFFFFFFFF'
  },
  ike3072 => { g => 2, p => 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1'.
                            '29024E088A67CC74020BBEA63B139B22514A08798E3404DD'.
                            'EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245'.
                            'E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED'.
                            'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D'.
                            'C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F'.
                            '83655D23DCA3AD961C62F356208552BB9ED529077096966D'.
                            '670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B'.
                            'E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9'.
                            'DE2BCBF6955817183995497CEA956AE515D2261898FA0510'.
                            '15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64'.
                            'ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7'.
                            'ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B'.
                            'F12FFA06D98A0864D87602733EC86A64521F2B18177B200C'.
                            'BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31'.
                            '43DB5BFCE0FD108E4B82D120A93AD2CAFFFFFFFFFFFFFFFF'
  },
  ike4096 => { g => 2, p => 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1'.
                            '29024E088A67CC74020BBEA63B139B22514A08798E3404DD'.
                            'EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245'.
                            'E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED'.
                            'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D'.
                            'C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F'.
                            '83655D23DCA3AD961C62F356208552BB9ED529077096966D'.
                            '670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B'.
                            'E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9'.
                            'DE2BCBF6955817183995497CEA956AE515D2261898FA0510'.
                            '15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64'.
                            'ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7'.
                            'ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B'.
                            'F12FFA06D98A0864D87602733EC86A64521F2B18177B200C'.
                            'BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31'.
                            '43DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D7'.
                            '88719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA'.
                            '2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6'.
                            '287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED'.
                            '1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA9'.
                            '93B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934063199'.
                            'FFFFFFFFFFFFFFFF'
  },
  ike6144 => { g => 2, p => 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1'.
                            '29024E088A67CC74020BBEA63B139B22514A08798E3404DD'.
                            'EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245'.
                            'E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED'.
                            'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D'.
                            'C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F'.
                            '83655D23DCA3AD961C62F356208552BB9ED529077096966D'.
                            '670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B'.
                            'E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9'.
                            'DE2BCBF6955817183995497CEA956AE515D2261898FA0510'.
                            '15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64'.
                            'ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7'.
                            'ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B'.
                            'F12FFA06D98A0864D87602733EC86A64521F2B18177B200C'.
                            'BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31'.
                            '43DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D7'.
                            '88719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA'.
                            '2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6'.
                            '287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED'.
                            '1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA9'.
                            '93B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934028492'.
                            '36C3FAB4D27C7026C1D4DCB2602646DEC9751E763DBA37BD'.
                            'F8FF9406AD9E530EE5DB382F413001AEB06A53ED9027D831'.
                            '179727B0865A8918DA3EDBEBCF9B14ED44CE6CBACED4BB1B'.
                            'DB7F1447E6CC254B332051512BD7AF426FB8F401378CD2BF'.
                            '5983CA01C64B92ECF032EA15D1721D03F482D7CE6E74FEF6'.
                            'D55E702F46980C82B5A84031900B1C9E59E7C97FBEC7E8F3'.
                            '23A97A7E36CC88BE0F1D45B7FF585AC54BD407B22B4154AA'.
                            'CC8F6D7EBF48E1D814CC5ED20F8037E0A79715EEF29BE328'.
                            '06A1D58BB7C5DA76F550AA3D8A1FBFF0EB19CCB1A313D55C'.
                            'DA56C9EC2EF29632387FE8D76E3C0468043E8F663F4860EE'.
                            '12BF2D5B0B7474D6E694F91E6DCC4024FFFFFFFFFFFFFFFF'
  },
  ike8192 => { g => 2, p => 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1'.
                            '29024E088A67CC74020BBEA63B139B22514A08798E3404DD'.
                            'EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245'.
                            'E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED'.
                            'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D'.
                            'C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F'.
                            '83655D23DCA3AD961C62F356208552BB9ED529077096966D'.
                            '670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B'.
                            'E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9'.
                            'DE2BCBF6955817183995497CEA956AE515D2261898FA0510'.
                            '15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64'.
                            'ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7'.
                            'ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B'.
                            'F12FFA06D98A0864D87602733EC86A64521F2B18177B200C'.
                            'BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31'.
                            '43DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D7'.
                            '88719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA'.
                            '2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6'.
                            '287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED'.
                            '1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA9'.
                            '93B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934028492'.
                            '36C3FAB4D27C7026C1D4DCB2602646DEC9751E763DBA37BD'.
                            'F8FF9406AD9E530EE5DB382F413001AEB06A53ED9027D831'.
                            '179727B0865A8918DA3EDBEBCF9B14ED44CE6CBACED4BB1B'.
                            'DB7F1447E6CC254B332051512BD7AF426FB8F401378CD2BF'.
                            '5983CA01C64B92ECF032EA15D1721D03F482D7CE6E74FEF6'.
                            'D55E702F46980C82B5A84031900B1C9E59E7C97FBEC7E8F3'.
                            '23A97A7E36CC88BE0F1D45B7FF585AC54BD407B22B4154AA'.
                            'CC8F6D7EBF48E1D814CC5ED20F8037E0A79715EEF29BE328'.
                            '06A1D58BB7C5DA76F550AA3D8A1FBFF0EB19CCB1A313D55C'.
                            'DA56C9EC2EF29632387FE8D76E3C0468043E8F663F4860EE'.
                            '12BF2D5B0B7474D6E694F91E6DBE115974A3926F12FEE5E4'.
                            '38777CB6A932DF8CD8BEC4D073B931BA3BC832B68D9DD300'.
                            '741FA7BF8AFC47ED2576F6936BA424663AAB639C5AE4F568'.
                            '3423B4742BF1C978238F16CBE39D652DE3FDB8BEFC848AD9'.
                            '22222E04A4037C0713EB57A81A23F0C73473FC646CEA306B'.
                            '4BCBC8862F8385DDFA9D4B7FA2C087E879683303ED5BDD3A'.
                            '062B3CF5B3A278A66D2A13F83F44F82DDF310EE074AB6A36'.
                            '4597E899A0255DC164F31CC50846851DF9AB48195DED7EA1'.
                            'B1D510BD7EE74D73FAF36BC31ECFA268359046F4EB879F92'.
                            '4009438B481C6CD7889A002ED5EE382BC9190DA6FC026E47'.
                            '9558E4475677E9AA9E3050E2765694DFC81F56E880B96E71'.
                            '60C980DD98EDD3DFFFFFFFFFFFFFFFFF'
  }
);

sub new {
  my $self = shift->_new();
  return @_ > 0 ? $self->import_key(@_) : $self;
}

sub import_key {
  my ($self, $key) = @_;
  croak "FATAL: undefined key" unless $key;
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
  croak "FATAL: invalid key format" unless $data;
  return $self->_import($data);
}

sub import_key_raw {
  my ($self, $raw_bytes, $type, $param) = @_;
  my ($g, $p, $x, $y);

  if (ref $param eq 'HASH') {
    $g = $param->{g} or croak "FATAL: 'g' param not specified";
    $p = $param->{p} or croak "FATAL: 'p' param not specified";
    $g =~ s/^0x//;
    $p =~ s/^0x//;
  } elsif (my $dhparam = $DH_PARAMS{$param}) {
    $g = $dhparam->{g};
    $p = $dhparam->{p};
  } else {
    croak "FATAL: invalid parameter";
  }

  if ($type eq 'private') {
    $type = 1;
  } elsif ($type eq 'public') {
    $type = 0;
  } else {
    croak "FATAL: invalid key type '$type'";
  }
  my $rv = $self->_import_raw($raw_bytes, $type, $g, $p);
  return $rv;
}

sub generate_key {
  my ($self, $param) = @_;

  if (!ref $param) {
    # group name
    return $self->_generate_key_gp($DH_PARAMS{$param}{g}, $DH_PARAMS{$param}{p}) if $DH_PARAMS{$param};
    # size
    return $self->_generate_key_size($param) if $param && $param =~ /^[0-9]+/;
  }
  elsif (ref $param eq 'SCALAR') {
    my $data = $$param;
    $data = pem_to_der($data) if $data =~ /-----BEGIN DH PARAMETERS-----\s*(.+)\s*-----END DH PARAMETERS-----/s;
    return $self->_generate_key_dhparam($data);
  }
  elsif (ref $param eq 'HASH') {
    my $g = $param->{g} or croak "FATAL: 'g' param not specified";
    my $p = $param->{p} or croak "FATAL: 'p' param not specified";
    $g =~ s/^0x//;
    $p =~ s/^0x//;
    return $self->_generate_key_gp($g, $p);
  }
  croak "FATAL: DH generate_key - invalid args";
}

### FUNCTIONS

sub dh_shared_secret {
  my ($privkey, $pubkey) = @_;
  $privkey = __PACKAGE__->new($privkey) unless ref $privkey;
  $pubkey  = __PACKAGE__->new($pubkey)  unless ref $pubkey;
  carp "FATAL: invalid 'privkey' param" unless ref($privkey) eq __PACKAGE__ && $privkey->is_private;
  carp "FATAL: invalid 'pubkey' param"  unless ref($pubkey)  eq __PACKAGE__;
  return $privkey->shared_secret($pubkey);
}

sub CLONE_SKIP { 1 } # prevent cloning

### DEPRECATED functions/methods

sub encrypt           { croak "Crypt::DH::encrypt is deprecated (removed in v0.049)" }
sub decrypt           { croak "Crypt::DH::decrypt is deprecated (removed in v0.049)" }
sub sign_message      { croak "Crypt::DH::sign_message is deprecated (removed in v0.049)" }
sub verify_message    { croak "Crypt::DH::verify_message is deprecated (removed in v0.049)" }
sub sign_hash         { croak "Crypt::DH::sign_hash is deprecated (removed in v0.049)" }
sub verify_hash       { croak "Crypt::DH::verify_hash is deprecated (removed in v0.049)" }
sub dh_encrypt        { croak "Crypt::DH::dh_encrypt is deprecated (removed in v0.049)" }
sub dh_decrypt        { croak "Crypt::DH::dh_decrypt is deprecated (removed in v0.049)" }
sub dh_sign_message   { croak "Crypt::DH::dh_sign_message is deprecated (removed in v0.049)" }
sub dh_verify_message { croak "Crypt::DH::dh_verify_message is deprecated (removed in v0.049)" }
sub dh_sign_hash      { croak "Crypt::DH::dh_sign_hash is deprecated (removed in v0.049)" }
sub dh_verify_hash    { croak "Crypt::DH::dh_verify_hash is deprecated (removed in v0.049)" }

1;

=pod

=head1 NAME

Crypt::PK::DH - Public key cryptography based on Diffie-Hellman

=head1 SYNOPSIS

 ### OO interface

 #Shared secret
 my $priv = Crypt::PK::DH->new('Alice_priv_dh1.key');
 my $pub = Crypt::PK::DH->new('Bob_pub_dh1.key');
 my $shared_secret = $priv->shared_secret($pub);

 #Key generation
 my $pk = Crypt::PK::DH->new();
 $pk->generate_key(128);
 my $private = $pk->export_key('private');
 my $public = $pk->export_key('public');

 or

 my $pk = Crypt::PK::DH->new();
 $pk->generate_key('ike2048');
 my $private = $pk->export_key('private');
 my $public = $pk->export_key('public');

 or

 my $pk = Crypt::PK::DH->new();
 $pk->generate_key({ p => $p, g => $g });
 my $private = $pk->export_key('private');
 my $public = $pk->export_key('public');

 ### Functional interface

 #Shared secret
 my $shared_secret = dh_shared_secret('Alice_priv_dh1.key', 'Bob_pub_dh1.key');

=head1 METHODS

=head2 new

  my $pk = Crypt::PK::DH->new();
  #or
  my $pk = Crypt::PK::DH->new($priv_or_pub_key_filename);
  #or
  my $pk = Crypt::PK::DH->new(\$buffer_containing_priv_or_pub_key);

=head2 generate_key

Uses Yarrow-based cryptographically strong random number generator seeded with
random data taken from C</dev/random> (UNIX) or C<CryptGenRandom> (Win32).

 $pk->generate_key($groupsize);
 ### $groupsize (in bytes) corresponds to DH parameters (p, g) predefined by libtomcrypt
 # 96   =>  DH-768
 # 128  =>  DH-1024
 # 192  =>  DH-1536
 # 256  =>  DH-2048
 # 384  =>  DH-3072
 # 512  =>  DH-4096
 # 768  =>  DH-6144
 # 1024 =>  DH-8192

The following variants are available since CryptX-0.032

 $pk->generate_key($groupname)
 ### $groupname corresponds to values defined in RFC7296 and RFC3526
 # 'ike768'  =>  768-bit MODP (Group 1)
 # 'ike1024' => 1024-bit MODP (Group 2)
 # 'ike1536' => 1536-bit MODP (Group 5)
 # 'ike2048' => 2048-bit MODP (Group 14)
 # 'ike3072' => 3072-bit MODP (Group 15)
 # 'ike4096' => 4096-bit MODP (Group 16)
 # 'ike6144' => 6144-bit MODP (Group 17)
 # 'ike8192' => 8192-bit MODP (Group 18)

 $pk->generate_key($param_hash)
 # $param_hash is { g => $g, p => $p }
 # where $g is the generator (base) in a hex string and $p is the prime in a hex string

 $pk->generate_key(\$dh_param)
 # $dh_param is the content of DER or PEM file with DH parameters
 # e.g. openssl dhparam 2048

=head2 import_key

Loads private or public key (exported by L</export_key>).

  $pk->import_key($filename);
  #or
  $pk->import_key(\$buffer_containing_key);

=head2 import_key_raw

I<Since: CryptX-0.032>

  $pk->import_key_raw($raw_bytes, $type, $params)
  ### $raw_bytes is a binary string containing the key
  ### $type is either 'private' or 'public'
  ### $param is either a name ('ike2038') or hash containing the p,g values { g=>$g, p=>$p }
  ### in hex strings

=head2 export_key

B<BEWARE:> DH key format change - since v0.049 it is compatible with libtomcrypt 1.18.

 my $private = $pk->export_key('private');
 #or
 my $public = $pk->export_key('public');

=head2 export_key_raw

I<Since: CryptX-0.032>

 $raw_bytes = $dh->export_key_raw('public')
 #or
 $raw_bytes = $dh->export_key_raw('private')

=head2 shared_secret

 # Alice having her priv key $pk and Bob's public key $pkb
 my $pk  = Crypt::PK::DH->new($priv_key_filename);
 my $pkb = Crypt::PK::DH->new($pub_key_filename);
 my $shared_secret = $pk->shared_secret($pkb);

 # Bob having his priv key $pk and Alice's public key $pka
 my $pk = Crypt::PK::DH->new($priv_key_filename);
 my $pka = Crypt::PK::DH->new($pub_key_filename);
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
   type => 0,   # integer: 1 .. private, 0 .. public
   size => 256, # integer: key size in bytes
   x => "FBC1062F73B9A17BB8473A2F5A074911FA7F20D28FB...", #private key
   y => "AB9AAA40774D3CD476B52F82E7EE2D8A8D40CD88BF4...", #public key
   g => "2", # generator/base
   p => "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80D...", # prime
}

=head2 params2hash

I<Since: CryptX-0.032>

 my $params = $pk->params2hash;

 # returns hash like this (or undef if no key loaded):
 {
   g => "2", # generator/base
   p => "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80D...", # prime
}

=head1 FUNCTIONS

=head2 dh_shared_secret

DH based shared secret generation. See method L</shared_secret> below.

 #on Alice side
 my $shared_secret = dh_shared_secret('Alice_priv_dh1.key', 'Bob_pub_dh1.key');

 #on Bob side
 my $shared_secret = dh_shared_secret('Bob_priv_dh1.key', 'Alice_pub_dh1.key');

=head1 DEPRECATED INTERFACE

The following functions/methods were removed in removed in v0.049:

 encrypt
 decrypt
 sign_message
 verify_message
 sign_hash
 verify_hash

 dh_encrypt
 dh_decrypt
 dh_sign_message
 dh_verify_message
 dh_sign_hash
 dh_verify_hash

=head1 SEE ALSO

=over

=item * L<https://en.wikipedia.org/wiki/Diffie%E2%80%93Hellman_key_exchange|https://en.wikipedia.org/wiki/Diffie%E2%80%93Hellman_key_exchange>

=back

=cut
