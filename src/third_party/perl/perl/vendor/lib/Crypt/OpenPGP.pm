package Crypt::OpenPGP;
use strict;
use 5.008_001;

use vars qw( $VERSION );
$VERSION = '1.06';

use Crypt::OpenPGP::Constants qw( DEFAULT_CIPHER );
use Crypt::OpenPGP::KeyRing;
use Crypt::OpenPGP::Plaintext;
use Crypt::OpenPGP::Message;
use Crypt::OpenPGP::PacketFactory;
use Crypt::OpenPGP::Config;

use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

use File::HomeDir;
use File::Spec;

use vars qw( %COMPAT );

## pgp2 and pgp5 do not trim trailing whitespace from "canonical text"
## signatures, only from cleartext signatures.
## See:
##   http://cert.uni-stuttgart.de/archive/ietf-openpgp/2000/01/msg00033.html
$Crypt::OpenPGP::Globals::Trim_trailing_ws = 1;

{
    my $env = sub {
        my $dir = shift; my @paths;
        if (exists $ENV{$dir}) { for (@_) { push @paths, "$ENV{$dir}/$_" } }
        return @paths ? @paths : ();
    };

    my $home = sub {
        my( @path ) = @_;
        my $home_dir = File::HomeDir->my_home or return;
        return File::Spec->catfile( $home_dir, @path );
    };

    %COMPAT = (
        PGP2 => {
              'sign'    => { Digest => 'MD5', Version => 3 },
              'encrypt' => { Cipher => 'IDEA', Compress => 'ZIP' },
              'keygen'  => { Type => 'RSA', Cipher => 'IDEA',
                             Version => 3, Digest => 'MD5' },
              'PubRing' => [
                     $env->('PGPPATH','pubring.pgp'),
                     $home->( '.pgp', 'pubring.pgp' ),
              ],
              'SecRing' => [
                     $env->('PGPPATH','secring.pgp'),
                     $home->( '.pgp', 'secring.pgp' ),
              ],
              'Config'  => [
                     $env->('PGPPATH', 'config.txt'),
                     $home->( '.pgp', 'config.txt' ),
              ],
        },

        PGP5 => {
              'sign'    => { Digest => 'SHA1', Version => 3 },
              'encrypt' => { Cipher => 'DES3', Compress => 'ZIP' },
              'keygen'  => { Type => 'DSA', Cipher => 'DES3',
                             Version => 4, Digest => 'SHA1' },
              'PubRing' => [
                     $env->('PGPPATH','pubring.pkr'),
                     $home->( '.pgp', 'pubring.pkr' ),
              ],
              'SecRing' => [
                     $env->('PGPPATH','secring.skr'),
                     $home->( '.pgp', 'secring.skr' ),
              ],
              'Config'  => [
                     $env->('PGPPATH', 'pgp.cfg'),
                     $home->( '.pgp', 'pgp.cfg' ),
              ],
        },

        GnuPG => {
              'sign'    => { Digest => 'RIPEMD160', Version => 4 },
              'encrypt' => { Cipher => 'Rijndael', Compress => 'Zlib',
                             MDC => 1 },
              'keygen'  => { Type => 'DSA', Cipher => 'Rijndael',
                             Version => 4, Digest => 'RIPEMD160' },
              'Config'  => [
                     $env->('GNUPGHOME', 'options'),
                     $home->( '.gnupg', 'options' ),
              ],
              'PubRing' => [
                     $env->('GNUPGHOME', 'pubring.gpg'),
                     $home->( '.gnupg', 'pubring.gpg' ),
              ],
              'SecRing' => [
                     $env->('GNUPGHOME', 'secring.gpg'),
                     $home->( '.gnupg', 'secring.gpg' ),
              ],
        },
    );
}

sub version_string { __PACKAGE__ . ' ' . $VERSION }

sub pubrings { $_[0]->{pubrings} }
sub secrings { $_[0]->{secrings} }

use constant PUBLIC => 1;
use constant SECRET => 2;

sub add_ring {
    my $pgp = shift;
    my($type, $ring) = @_;
    unless (ref($ring) eq 'Crypt::OpenPGP::KeyRing') {
        $ring = Crypt::OpenPGP::KeyRing->new( Filename => $ring )
            or return Crypt::OpenPGP::KeyRing->errstr;
    }
    if ($type == SECRET) {
        push @{ $pgp->{secrings} }, $ring;
    } else {
        push @{ $pgp->{pubrings} }, $ring;
    }
    $ring;
}

sub new {
    my $class = shift;
    my $pgp = bless { }, $class;
    $pgp->init(@_);
}

sub _first_exists {
    my($list) = @_;
    for my $f (@$list) {
        next unless $f;
        return $f if -e $f;
    }
}

sub init {
    my $pgp = shift;
    $pgp->{pubrings} = [];
    $pgp->{secrings} = [];
    my %param = @_;
    my $cfg_file = delete $param{ConfigFile};
    my $cfg = $pgp->{cfg} = Crypt::OpenPGP::Config->new(%param) or
        return Crypt::OpenPGP::Config->errstr;
    if (!$cfg_file && (my $compat = $cfg->get('Compat'))) {
        $cfg_file = _first_exists($COMPAT{$compat}{Config});
    }
    if ($cfg_file) {
        $cfg->read_config($param{Compat}, $cfg_file);
    }
    ## Load public and secret keyrings.
    for my $s (qw( PubRing SecRing )) {
        unless (defined $cfg->get($s)) {
            my @compats = $param{Compat} ? ($param{Compat}) : keys %COMPAT;
            for my $compat (@compats) {
                my $ring = _first_exists($COMPAT{$compat}{$s});
                $cfg->set($s, $ring), last if $ring;
            }
        }
        if (my $ring = $cfg->get($s)) {
            $pgp->add_ring($s eq 'PubRing' ? PUBLIC : SECRET, $ring);
        }
    }
    $pgp;
}

sub handle {
    my $pgp = shift;
    my %param = @_;
    my($data);
    unless ($data = $param{Data}) {
        my $file = $param{Filename} or
            return $pgp->error("Need either 'Data' or 'Filename' to decrypt");
        $data = $pgp->_read_files($file) or return $pgp->error($pgp->errstr);
    }
    my $msg = Crypt::OpenPGP::Message->new( Data => $data ) or
        return $pgp->error("Reading data packets failed: " .
            Crypt::OpenPGP::Message->errstr);
    my @pieces = $msg->pieces;
    return $pgp->error("No packets found in message") unless @pieces;
    while (ref($pieces[0]) eq 'Crypt::OpenPGP::Marker') {
        shift @pieces;
    }
    if (ref($pieces[0]) eq 'Crypt::OpenPGP::Compressed') {
        $data = $pieces[0]->decompress or
            return $pgp->error("Decompression error: " . $pieces[0]->errstr);
        $msg = Crypt::OpenPGP::Message->new( Data => $data ) or
            return $pgp->error("Reading decompressed data failed: " .
                Crypt::OpenPGP::Message->errstr);
        @pieces = $msg->pieces;
    }
    my $class = ref($pieces[0]);
    my(%res);
    if ($class eq 'Crypt::OpenPGP::OnePassSig' ||
        $class eq 'Crypt::OpenPGP::Signature') {
        my($valid, $sig) = $pgp->verify( Signature => $data );
        return $pgp->error("Error verifying signature: " . $pgp->errstr)
            if !defined $valid;
        $res{Validity} = $valid;
        $res{Signature} = $sig;
    } else {
        my $cb = $param{PassphraseCallback} || \&_default_passphrase_cb;
        my($pt, $valid, $sig) = $pgp->decrypt(
                      Data => $data,
                      PassphraseCallback => $cb,
                 );
        return $pgp->error("Decryption failed: " . $pgp->errstr)
            unless defined $pt;
        return $pgp->error("Error verifying signature: " . $pgp->errstr)
            if !defined($valid) && $pgp->errstr !~ /^No Signature/;
        $res{Plaintext} = $pt;
        $res{Validity} = $valid if defined $valid;
        $res{Signature} = $sig if defined $sig;
    }
    \%res;
}

sub _default_passphrase_cb {
    my($cert) = @_;
    my $prompt;
    if ($cert) {
        $prompt = sprintf qq(
You need a passphrase to unlock the secret key for
user "%s".
%d-bit %s key, ID %s

Enter passphrase: ), $cert->uid,
                     $cert->key->size,
                     $cert->key->alg,
                     substr($cert->key_id_hex, -8, 8);
    } else {
        $prompt = "Enter passphrase: ";
    }
    _prompt($prompt, '', 1);
}

sub _prompt {
    my($prompt, $def, $noecho) = @_;
    require Term::ReadKey;
    Term::ReadKey->import;
    print STDERR $prompt . ($def ? "[$def] " : "");
    if ($noecho) {
        ReadMode('noecho');
    }
    chomp(my $ans = ReadLine(0));
    ReadMode('restore');
    print STDERR "\n";
    $ans ? $ans : $def;
}

sub sign {
    my $pgp = shift;
    my %param = @_;
    $pgp->_merge_compat(\%param, 'sign') or
        return $pgp->error( $pgp->errstr );
    my($cert, $data);
    require Crypt::OpenPGP::Signature;
    unless ($data = $param{Data}) {
        my $file = $param{Filename} or
            return $pgp->error("Need either 'Data' or 'Filename' to sign");
        $data = $pgp->_read_files($file) or return $pgp->error($pgp->errstr);
    }
    unless ($cert = $param{Key}) {
        my $kid = $param{KeyID} or return $pgp->error("No KeyID specified");
        my $ring = $pgp->secrings->[0]
            or return $pgp->error("No secret keyrings");
        my $kb = $ring->find_keyblock_by_keyid(pack 'H*', $kid) or
            return $pgp->error("Could not find secret key with KeyID $kid");
        $cert = $kb->signing_key;
        $cert->uid($kb->primary_uid);
    }
    if ($cert->is_protected) {
        my $pass = $param{Passphrase};
        if (!defined $pass && (my $cb = $param{PassphraseCallback})) {
            $pass = $cb->($cert);
        }
        return $pgp->error("Need passphrase to unlock secret key")
            unless $pass;
        $cert->unlock($pass) or
            return $pgp->error("Secret key unlock failed: " . $cert->errstr);
    }
    my @ptarg;
    push @ptarg, ( Filename => $param{Filename} ) if $param{Filename};
    if ($param{Clearsign}) {
        push @ptarg, ( Mode => 't' );
        ## In clear-signed messages, the line ending before the signature
        ## is not considered part of the signed text.
        (my $tmp = $data) =~ s!\r?\n$!!;
        push @ptarg, ( Data => $tmp );
    } else {
        push @ptarg, ( Data => $data );
    }
    my $pt = Crypt::OpenPGP::Plaintext->new(@ptarg);
    my @sigarg;
    if (my $hash_alg = $param{Digest}) {
        my $dgst = Crypt::OpenPGP::Digest->new($hash_alg) or
            return $pgp->error( Crypt::OpenPGP::Digest->errstr );
        @sigarg = ( Digest => $dgst->alg_id );
    }
    push @sigarg, (Type => 0x01) if $param{Clearsign};
    my $sig = Crypt::OpenPGP::Signature->new(
                          Data => $pt,
                          Key  => $cert,
                          Version => $param{Version},
                          @sigarg,
                 );
    if ($param{Clearsign}) {
        $param{Armour} = $param{Detach} = 1;
    }
    my $sig_data = Crypt::OpenPGP::PacketFactory->save($sig,
        $param{Detach} ? () : ($pt));
    if ($param{Armour}) {
        require Crypt::OpenPGP::Armour;
        $sig_data = Crypt::OpenPGP::Armour->armour(
                          Data => $sig_data,
                          Object => ($param{Detach} ? 'SIGNATURE' : 'MESSAGE'),
                 ) or return $pgp->error( Crypt::OpenPGP::Armour->errstr );
    }
    if ($param{Clearsign}) {
        require Crypt::OpenPGP::Util;
        my $hash = Crypt::OpenPGP::Digest->alg($sig->{hash_alg});
        my $data = Crypt::OpenPGP::Util::dash_escape($data);
        $data .= "\n" unless $data =~ /\n$/;
        $sig_data = "-----BEGIN PGP SIGNED MESSAGE-----\n" .
                    ($hash eq 'MD5' ? '' : "Hash: $hash\n") .
                    "\n" .
                    $data .
                    $sig_data;
    }
    $sig_data;
}

sub verify {
    my $pgp = shift;
    my %param = @_;
    my $wants_object = wantarray;
    my($data, $sig);
    require Crypt::OpenPGP::Signature;
    $param{Signature} or $param{SigFile} or
            return $pgp->error("Need Signature or SigFile to verify");
    my %arg = $param{Signature} ? (Data => $param{Signature}) :
                                  (Filename => $param{SigFile});
    $arg{IsPacketStream} = 1 if $param{IsPacketStream};
    my $msg = Crypt::OpenPGP::Message->new( %arg ) or
        return $pgp->error("Reading signature failed: " .
            Crypt::OpenPGP::Message->errstr);
    my @pieces = $msg->pieces;
    if (ref($pieces[0]) eq 'Crypt::OpenPGP::Compressed') {
        $data = $pieces[0]->decompress or
            return $pgp->error("Decompression error: " . $pieces[0]->errstr);
        $msg = Crypt::OpenPGP::Message->new( Data => $data ) or
            return $pgp->error("Reading decompressed data failed: " .
                Crypt::OpenPGP::Message->errstr);
        @pieces = $msg->pieces;
    }
    if (ref($pieces[0]) eq 'Crypt::OpenPGP::OnePassSig') {
        ($data, $sig) = @pieces[1,2];
    } elsif (ref($pieces[0]) eq 'Crypt::OpenPGP::Signature') {
        ($sig, $data) = @pieces[0,1];
    } else {
        return $pgp->error("SigFile contents are strange");
    }
    unless ($data) {
        if ($param{Data}) {
            $data = Crypt::OpenPGP::Plaintext->new( Data => $param{Data} );
        }
        else {
            ## if no Signature or detached sig in SigFile
            my @files = ref($param{Files}) eq 'ARRAY' ? @{ $param{Files} } :
                            $param{Files};
            my $fdata = $pgp->_read_files(@files);
            return $pgp->error("Reading data files failed: " . $pgp->errstr)
                unless defined $fdata;
            $data = Crypt::OpenPGP::Plaintext->new( Data => $fdata );
       }
    }
    my($cert, $kb);
    unless ($cert = $param{Key}) {
        my $key_id = $sig->key_id;
        my $ring = $pgp->pubrings->[0];
        unless ($ring && ($kb = $ring->find_keyblock_by_keyid($key_id))) {
            my $cfg = $pgp->{cfg};
            if ($cfg->get('AutoKeyRetrieve') && $cfg->get('KeyServer')) {
                require Crypt::OpenPGP::KeyServer;
                my $server = Crypt::OpenPGP::KeyServer->new(
                                Server => $cfg->get('KeyServer'),
                          );
                $kb = $server->find_keyblock_by_keyid($key_id);
            }
            return $pgp->error("Could not find public key with KeyID " .
                unpack('H*', $key_id))
                unless $kb;
        }
        $cert = $kb->signing_key;
    }

## pgp2 and pgp5 do not trim trailing whitespace from "canonical text"
## signatures, only from cleartext signatures. So we first try to verify
## the signature using proper RFC2440 canonical text, then if that fails,
## retry without trimming trailing whitespace.
## See:
##   http://cert.uni-stuttgart.de/archive/ietf-openpgp/2000/01/msg00033.html
    my($dgst, $found);
    for (1, 0) {
        local $Crypt::OpenPGP::Globals::Trim_trailing_ws = $_;
        $dgst = $sig->hash_data($data) or
            return $pgp->error( $sig->errstr );
        $found++, last if substr($dgst, 0, 2) eq $sig->{chk};
    }
    return $pgp->error("Message hash does not match signature checkbytes")
        unless $found;
    my $valid = $cert->key->public_key->verify($sig, $dgst) ?
      ($kb && $kb->primary_uid ? $kb->primary_uid : 1) : 0;

    $wants_object ? ($valid, $sig) : $valid;
}

sub encrypt {
    my $pgp = shift;
    my %param = @_;
    $pgp->_merge_compat(\%param, 'encrypt') or
        return $pgp->error( $pgp->errstr );
    my($data);
    require Crypt::OpenPGP::Cipher;
    require Crypt::OpenPGP::Ciphertext;
    unless ($data = $param{Data}) {
        my $file = $param{Filename} or
            return $pgp->error("Need either 'Data' or 'Filename' to encrypt");
        $data = $pgp->_read_files($file) or return $pgp->error($pgp->errstr);
    }
    my $ptdata;
    if ($param{SignKeyID}) {
        $ptdata = $pgp->sign(
                         Data       => $data,
                         KeyID      => $param{SignKeyID},
                         Compat     => $param{Compat},
                         Armour     => 0,
                         Passphrase => $param{SignPassphrase},
                         PassphraseCallback => $param{SignPassphraseCallback},
                  )
            or return;
    } else {
        my $pt = Crypt::OpenPGP::Plaintext->new( Data => $data,
                      $param{Filename} ? (Filename => $param{Filename}) : () );
        $ptdata = Crypt::OpenPGP::PacketFactory->save($pt);
    }
    if (my $alg = $param{Compress}) {
        require Crypt::OpenPGP::Compressed;
        $alg = Crypt::OpenPGP::Compressed->alg_id($alg);
        my $cdata = Crypt::OpenPGP::Compressed->new( Data => $ptdata,
            Alg => $alg ) or return $pgp->error("Compression error: " .
                Crypt::OpenPGP::Compressed->errstr);
        $ptdata = Crypt::OpenPGP::PacketFactory->save($cdata);
    }
    require Crypt::Random;
    my $key_data = Crypt::Random::makerandom_octet( Length => 32 );
    my $sym_alg = $param{Cipher} ?
        Crypt::OpenPGP::Cipher->alg_id($param{Cipher}) : DEFAULT_CIPHER;
    my(@sym_keys);
    if ($param{Recipients} && !ref($param{Recipients})) {
        $param{Recipients} = [ $param{Recipients} ];
    }
    if (my $kid = delete $param{KeyID}) {
        my @kid = ref $kid eq 'ARRAY' ? @$kid : $kid;
        push @{ $param{Recipients} }, @kid;
    }
    if ($param{Key} || $param{Recipients}) {
        require Crypt::OpenPGP::SessionKey;
        my @keys;
        if (my $recips = $param{Recipients}) {
            my @recips = ref $recips eq 'ARRAY' ? @$recips : $recips;
            my $ring = $pgp->pubrings->[0];
            my %seen;
            my $server;
            my $cfg = $pgp->{cfg};
            if ($cfg->get('AutoKeyRetrieve') && $cfg->get('KeyServer')) {
                require Crypt::OpenPGP::KeyServer;
                $server = Crypt::OpenPGP::KeyServer->new(
                                Server => $cfg->get('KeyServer'),
                          );
            }
            for my $r (@recips) {
                my($lr, @kb) = (length($r));
                if (($lr == 8 || $lr == 16) && $r !~ /[^\da-fA-F]/) {
                    my $id = pack 'H*', $r;
                    @kb = $ring->find_keyblock_by_keyid($id) if $ring;
                    @kb = $server->find_keyblock_by_keyid($id)
                        if !@kb && $server;
                } else {
                    @kb = $ring->find_keyblock_by_uid($r) if $ring;
                    @kb = $server->find_keyblock_by_uid($r)
                        if !@kb && $server;
                }
                for my $kb (@kb) {
                    next unless my $cert = $kb->encrypting_key;
                    next if $seen{ $cert->key_id }++;
                    $cert->uid($kb->primary_uid);
                    push @keys, $cert;
                }
            }
            if (my $cb = $param{RecipientsCallback}) {
                @keys = @{ $cb->(\@keys) };
            }
        }
        if ($param{Key}) {
            push @keys, ref $param{Key} eq 'ARRAY' ? @{$param{Key}} :
                                                       $param{Key};
        }
        return $pgp->error("No known recipients for encryption")
            unless @keys;
        for my $key (@keys) {
            push @sym_keys, Crypt::OpenPGP::SessionKey->new(
                                Key    => $key,
                                SymKey => $key_data,
                                Cipher => $sym_alg,
                          ) or
                return $pgp->error( Crypt::OpenPGP::SessionKey->errstr );
        }
    }
    elsif (my $pass = $param{Passphrase}) {
        require Crypt::OpenPGP::SKSessionKey;
        require Crypt::OpenPGP::S2k;
        my $s2k;
        if ($param{Compat} && $param{Compat} eq 'PGP2') {
            $s2k = Crypt::OpenPGP::S2k->new('Simple');
            $s2k->{hash} = Crypt::OpenPGP::Digest->new('MD5');
        } else {
            $s2k = Crypt::OpenPGP::S2k->new('Salt_Iter');
        }
        my $keysize = Crypt::OpenPGP::Cipher->new($sym_alg)->keysize;
        $key_data = $s2k->generate($pass, $keysize);
        push @sym_keys, Crypt::OpenPGP::SKSessionKey->new(
                            Passphrase => $pass,
                            SymKey     => $key_data,
                            Cipher     => $sym_alg,
                            S2k        => $s2k,
                      ) or
            return $pgp->error( Crypt::OpenPGP::SKSessionKey->errstr );
    } else {
        return $pgp->error("Need something to encrypt with");
    }
    my $enc = Crypt::OpenPGP::Ciphertext->new(
                        MDC    => $param{MDC},
                        SymKey => $key_data,
                        Data   => $ptdata,
                        Cipher => $sym_alg,
                  );
    my $enc_data = Crypt::OpenPGP::PacketFactory->save(
        $param{Passphrase} && $param{Compat} && $param{Compat} eq 'PGP2' ?
        $enc : (@sym_keys, $enc)
    );
    if ($param{Armour}) {
        require Crypt::OpenPGP::Armour;
        $enc_data = Crypt::OpenPGP::Armour->armour(
                          Data => $enc_data,
                          Object => 'MESSAGE',
                 ) or return $pgp->error( Crypt::OpenPGP::Armour->errstr );
    }
    $enc_data;
}

sub decrypt {
    my $pgp = shift;
    my %param = @_;
    my $wants_verify = wantarray;
    my($data);
    unless ($data = $param{Data}) {
        my $file = $param{Filename} or
            return $pgp->error("Need either 'Data' or 'Filename' to decrypt");
        $data = $pgp->_read_files($file) or return $pgp->error($pgp->errstr);
    }
    my $msg = Crypt::OpenPGP::Message->new( Data => $data ) or
        return $pgp->error("Reading data packets failed: " .
            Crypt::OpenPGP::Message->errstr);
    my @pieces = $msg->pieces;
    return $pgp->error("No packets found in message") unless @pieces;
    while (ref($pieces[0]) eq 'Crypt::OpenPGP::Marker') {
        shift @pieces;
    }
    my($key, $alg);
    if (ref($pieces[0]) eq 'Crypt::OpenPGP::SessionKey') {
        my($sym_key, $cert, $ring) = (shift @pieces);
        unless ($cert = $param{Key}) {
            $ring = $pgp->secrings->[0]
                or return $pgp->error("No secret keyrings");
        }
        my($kb);
        while (ref($sym_key) eq 'Crypt::OpenPGP::SessionKey') {
            if ($cert) {
                if ($cert->key_id eq $sym_key->key_id) {
                    shift @pieces
                        while ref($pieces[0]) eq 'Crypt::OpenPGP::SessionKey';
                    last;
                }
            } else {
                if ($kb = $ring->find_keyblock_by_keyid($sym_key->key_id)) {
                    shift @pieces
                        while ref($pieces[0]) eq 'Crypt::OpenPGP::SessionKey';
                    last;
                }
            }
            $sym_key = shift @pieces;
        }
        return $pgp->error("Can't find a secret key to decrypt message")
            unless $kb || $cert;
        if ($kb) {
            $cert = $kb->encrypting_key;
            $cert->uid($kb->primary_uid);
        }
        if ($cert->is_protected) {
            my $pass = $param{Passphrase};
            if (!defined $pass && (my $cb = $param{PassphraseCallback})) {
                $pass = $cb->($cert);
            }
            return $pgp->error("Need passphrase to unlock secret key")
                unless $pass;
            $cert->unlock($pass) or
                return $pgp->error("Seckey unlock failed: " . $cert->errstr);
        }
        ($key, $alg) = $sym_key->decrypt($cert) or
            return $pgp->error("Symkey decrypt failed: " . $sym_key->errstr);
    } 
    elsif (ref($pieces[0]) eq 'Crypt::OpenPGP::SKSessionKey') {
        my $sym_key = shift @pieces;
        my $pass = $param{Passphrase};
        if (!defined $pass && (my $cb = $param{PassphraseCallback})) {
            $pass = $cb->();
        }
        return $pgp->error("Need passphrase to decrypt session key")
            unless $pass;
        ($key, $alg) = $sym_key->decrypt($pass) or
            return $pgp->error("Symkey decrypt failed: " . $sym_key->errstr);
    }
    my $enc = $pieces[0];

    ## If there is still no symkey and symmetric algorithm, *and* the
    ## first packet is a Crypt::OpenPGP::Ciphertext packet, assume that
    ## the packet is encrypted using a symmetric key, using a 'Simple' s2k.
    if (!$key && !$alg && ref($enc) eq 'Crypt::OpenPGP::Ciphertext') {
        my $pass = $param{Passphrase} or
            return $pgp->error("Need passphrase to decrypt session key");
        require Crypt::OpenPGP::Cipher;
        require Crypt::OpenPGP::S2k;
        my $ciph = Crypt::OpenPGP::Cipher->new('IDEA');
        my $s2k = Crypt::OpenPGP::S2k->new('Simple');
        $s2k->{hash} = Crypt::OpenPGP::Digest->new('MD5');
        $key = $s2k->generate($pass, $ciph->keysize);
        $alg = $ciph->alg_id;
    }

    $data = $enc->decrypt($key, $alg) or
        return $pgp->error("Ciphertext decrypt failed: " . $enc->errstr);

    ## This is a special hack: if decrypt gets a signed, encrypted message,
    ## it needs to be able to pass back the decrypted text *and* a flag
    ## saying whether the signature is valid or not. But in some cases,
    ## you don't know ahead of time if there is a signature at all--and if
    ## there isn't, there is no way of knowing whether the signature is valid,
    ## or if there isn't a signature at all. So this prepopulates the internal
    ## errstr with the string "No Signature\n"--if there is a signature, and
    ## there is an error during verification, the second return value will be
    ## undef, and the errstr will contain the error that occurred. If there is
    ## *not* a signature, the second return value will still be undef, but
    ## the errstr is guaranteed to be "No Signature\n".
    $pgp->error("No Signature");

    my($valid, $sig);
    $msg = Crypt::OpenPGP::Message->new( Data => $data,
                                         IsPacketStream => 1 );
    @pieces = $msg->pieces;

    ## If the first packet in the decrypted data is compressed,
    ## decompress it and set the list of packets to the result.
    if (ref($pieces[0]) eq 'Crypt::OpenPGP::Compressed') {
        $data = $pieces[0]->decompress or
            return $pgp->error("Decompression error: " . $pieces[0]->errstr);
        $msg = Crypt::OpenPGP::Message->new( Data => $data,
                                             IsPacketStream => 1 );
        @pieces = $msg->pieces;
    }

    my($pt);
    if (ref($pieces[0]) eq 'Crypt::OpenPGP::OnePassSig' ||
        ref($pieces[0]) eq 'Crypt::OpenPGP::Signature') {
        $pt = $pieces[1];
        if ($wants_verify) {
            ($valid, $sig) =
                $pgp->verify( Signature => $data, IsPacketStream => 1 );
        }
    } else {
        $pt = $pieces[0];
    }

    $wants_verify ? ($pt->data, $valid, $sig) : $pt->data;
}

sub keygen {
    my $pgp = shift;
    my %param = @_;
    require Crypt::OpenPGP::Certificate;
    require Crypt::OpenPGP::Key;
    require Crypt::OpenPGP::KeyBlock;
    require Crypt::OpenPGP::Signature;
    require Crypt::OpenPGP::UserID;

    $param{Type} or
        return $pgp->error("Need a Type of key to generate");
    $param{Size} ||= 1024;
    $param{Version} ||= 4;
    $param{Version} = 3 if $param{Type} eq 'RSA';

    my $kb_pub = Crypt::OpenPGP::KeyBlock->new;
    my $kb_sec = Crypt::OpenPGP::KeyBlock->new;

    my($pub, $sec) = Crypt::OpenPGP::Key->keygen($param{Type}, %param);
    die Crypt::OpenPGP::Key->errstr unless $pub && $sec;
    my $pubcert = Crypt::OpenPGP::Certificate->new(
                             Key        => $pub,
                             Version    => $param{Version}
                ) or
        die Crypt::OpenPGP::Certificate->errstr;
    my $seccert = Crypt::OpenPGP::Certificate->new(
                             Key        => $sec,
                             Passphrase => $param{Passphrase},
                             Version    => $param{Version}
                ) or
        die Crypt::OpenPGP::Certificate->errstr;
    $kb_pub->add($pubcert);
    $kb_sec->add($seccert);

    my $id = Crypt::OpenPGP::UserID->new( Identity => $param{Identity} );
    $kb_pub->add($id);
    $kb_sec->add($id);

    my $sig = Crypt::OpenPGP::Signature->new(
                             Data    => [ $pubcert, $id ],
                             Key     => $seccert,
                             Version => $param{Version},
                             Type    => 0x13,
               );
    $kb_pub->add($sig);
    $kb_sec->add($sig);

    ($kb_pub, $kb_sec);
}

sub _read_files {
    my $pgp = shift;
    return $pgp->error("No files specified") unless @_;
    my @files = @_;
    my $data = '';
    for my $file (@files) {
        $file ||= '';
        local *FH;
        open FH, $file or return $pgp->error("Error opening $file: $!");
        binmode FH;
        { local $/; $data .= <FH> }
        close FH or warn "Warning: Got error closing $file: $!";
    }
    $data;
}

{
    my @MERGE_CONFIG = qw( Cipher Armour Digest );
    sub _merge_compat {
        my $pgp = shift;
        my($param, $meth) = @_;
        my $compat = $param->{Compat} || $pgp->{cfg}->get('Compat') || return 1;
        my $ref = $COMPAT{$compat}{$meth} or
            return $pgp->error("No settings for Compat class '$compat'");
        for my $arg (keys %$ref) {
            $param->{$arg} = $ref->{$arg} unless exists $param->{$arg};
        }
        for my $key (@MERGE_CONFIG) {
            $param->{$key} = $pgp->{cfg}->get($key)
                unless exists $param->{$key};
        }
        1;
    }
}

1;

__END__

=head1 NAME

Crypt::OpenPGP - Pure-Perl OpenPGP implementation

=head1 SYNOPSIS

    my $pgp = Crypt::OpenPGP->new;

    # Given an input stream (could be a signature, ciphertext, etc),
    # do the "right thing" to it.
    my $message_body; $message_body .= $_ while <STDIN>;
    my $result = $pgp->handle( Data => $message_body );

    # Create a detached, ASCII-armoured signature of $file using the
    # secret key $key_id, protected with the passphrase $pass. 
    my $file = 'really-from-me.txt';
    my $key_id = '...';
    my $pass = 'foo bar';
    my $signature = $pgp->sign(
        Filename   => $file,
        KeyID      => $key_id,
        Passphrase => $pass,
        Detach     => 1,
        Armour     => 1,
    );

    # Verify the detached signature $signature, which should be of the
    # source file $file.
    my $is_valid = $pgp->verify(
        Signature  => $signature,
        Files      => [ $file ],
    );

    # Using the public key associated with $key_id, encrypt the contents
    # of the file $file, and ASCII-armour the ciphertext.
    my $ciphertext = $pgp->encrypt(
        Filename   => $file,
        Recipients => $key_id,
        Armour     => 1,
    );

    # Decrypt $ciphertext using the secret key used to encrypt it,
    # which key is protected with the passphrase $pass.
    my $plaintext = $pgp->decrypt(
        Data       => $ciphertext,
        Passphrase => $pass,
    );

=head1 DESCRIPTION

I<Crypt::OpenPGP> is a pure-Perl implementation of the OpenPGP
standard[1]. In addition to support for the standard itself,
I<Crypt::OpenPGP> claims compatibility with many other PGP implementations,
both those that support the standard and those that preceded it.

I<Crypt::OpenPGP> provides signing/verification, encryption/decryption,
keyring management, and key-pair generation; in short it should provide
you with everything you need to PGP-enable yourself. Alternatively it
can be used as part of a larger system; for example, perhaps you have
a web-form-to-email generator written in Perl, and you'd like to encrypt
outgoing messages, because they contain sensitive information.
I<Crypt::OpenPGP> can be plugged into such a scenario, given your public
key, and told to encrypt all messages; they will then be readable only
by you.

This module currently supports C<RSA> and C<DSA> for digital signatures,
and C<RSA> and C<ElGamal> for encryption/decryption. It supports the
symmetric ciphers C<3DES>, C<Blowfish>, C<IDEA>, C<Twofish>, C<CAST5>, and
C<Rijndael> (C<AES>). C<Rijndael> is supported for key sizes of C<128>,
C<192>, and C<256> bits. I<Crypt::OpenPGP> supports the digest algorithms
C<MD5>, C<SHA-1>, and C<RIPE-MD/160>. And it supports C<ZIP> and C<Zlib>
compression.

=head1 COMPATIBILITY

One of the highest priorities for I<Crypt::OpenPGP> is compatibility with
other PGP implementations, including PGP implementations that existed
before the OpenPGP standard.

As a means towards that end, some of the high-level I<Crypt::OpenPGP>
methods can be used in compatibility mode; given an argument I<Compat>
and a PGP implementation with which they should be compatible, these
method will do their best to choose ciphers, digest algorithms, etc. that
are compatible with that implementation. For example, PGP2 only supports
C<IDEA> encryption, C<MD5> digests, and version 3 signature formats; if
you tell I<Crypt::OpenPGP> that it must be compatible with PGP2, it will
only use these algorithms/formats when encrypting and signing data.

To use this feature, supply either I<sign> or I<encrypt> with the
I<Compat> parameter, giving it one of the values from the list below.
For example:

    my $ct = $pgp->encrypt(
                  Compat     => 'PGP2',
                  Filename   => 'foo.pl',
                  Recipients => $key_id,
             );

Because I<PGP2> was specified, the data will automatically be encrypted
using the C<IDEA> cipher, and will be compressed using C<ZIP>.

Here is a list of the current compatibility sets and the algorithms and
formats they support.

=over 4

=item * PGP2

Encryption: symmetric cipher = C<IDEA>, compression = C<ZIP>,
modification detection code (MDC) = C<0>

Signing: digest = C<MD5>, packet format = version 3

=item * PGP5

Encryption: symmetric cipher = C<3DES>, compression = C<ZIP>,
modification detection code (MDC) = C<0>

Signing: digest = C<SHA-1>, packet format = version 3

=item * GnuPG

Encryption: symmetric cipher = C<Rijndael>, compression = C<Zlib>,
modification detection code (MDC) = C<1>

Signing: digest = C<RIPE-MD/160>, packet format = version 4

=back

If the compatibility setting is unspecified (that is, if no I<Compat>
argument is supplied), the settings (ciphers, digests, etc.) fall
back to their default settings.

=head1 USAGE

I<Crypt::OpenPGP> has the following high-level interface. On failure,
all methods will return C<undef> and set the I<errstr> for the object;
look below at the I<ERROR HANDLING> section for more information.

=head2 Crypt::OpenPGP->new( %args )

Constructs a new I<Crypt::OpenPGP> instance and returns that object.
Returns C<undef> on failure.

I<%args> can contain:

=over 4

=item * Compat

The compatibility mode for this I<Crypt::OpenPGP> object. This value will
propagate down into method calls upon this object, meaning that it will be
applied for all method calls invoked on this object. For example, if you set
I<Compat> here, you do not have to set it again when calling I<encrypt>
or I<sign> (below), unless, of course, you want to set I<Compat> to a
different value for those methods.

I<Compat> influences several factors upon object creation, unless otherwise
overridden in the constructor arguments: if you have a configuration file
for this compatibility mode (eg. F<~/.gnupg/options> for GnuPG), it will
be automatically read in, and I<Crypt::OpenPGP> will set any options
relevant to its execution (symmetric cipher algorithm, etc.); I<PubRing>
and I<SecRing> (below) are set according to the default values for this
compatibility mode (eg. F<~/.gnupg/pubring.gpg> for the GnuPG public
keyring).

=item * SecRing

Path to your secret keyring. If unspecified, I<Crypt::OpenPGP> will look
for your keyring in a number of default places.

As an alternative to passing in a path to the keyring file, you can pass in
a I<Crypt::OpenPGP::KeyRing> object representing a secret keyring.

=item * PubRing

Path to your public keyring. If unspecified, I<Crypt::OpenPGP> will look
for your keyring in a number of default places.

As an alternative to passing in a path to the keyring file, you can pass in
a I<Crypt::OpenPGP::KeyRing> object representing a public keyring.

=item * ConfigFile

Path to a PGP/GnuPG config file. If specified, you must also pass in a
value for the I<Compat> parameter, stating what format config file you are
passing in. For example, if you are passing in the path to a GnuPG config
file, you should give a value of C<GnuPG> for the I<Compat> flag.

If you leave I<ConfigFile> unspecified, but you have specified a value for
I<Compat>, I<Crypt::OpenPGP> will try to find your config file, based on
the value of I<Compat> that you pass in (eg. F<~/.gnupg/options> if
I<Compat> is C<GnuPG>).

NOTE: if you do not specify a I<Compat> flag, I<Crypt::OpenPGP> cannot read
any configuration files, even if you I<have> specified a value for the
I<ConfigFile> parameter, because it will not be able to determine the proper
config file format.

=item * KeyServer

The hostname of the HKP keyserver. You can get a list of keyservers through

    % host -l pgp.net | grep wwwkeys

If I<AutoKeyRetrieve> is set to a true value,
keys will be automatically retrieved from the keyserver if they are not found
in your local keyring.

=item * AutoKeyRetrieve

If set to a true value, and if I<KeyServer> is set to a keyserver name,
I<encrypt> and I<verify> will automatically try to fetch public keys from
the keyserver if they are not found in your local keyring.

=back

=head2 $pgp->handle( %args )

A do-what-I-mean wrapper around I<decrypt> and I<verify>. Given either a
filename or a block of data--for example, data from an incoming email
message--I<handle> "handles" it as appropriate for whatever encryption or
signing the message contains. For example, if the data is encrypted, I<handle>
will return the decrypted data (after prompting you for the passphrase). If
the data is signed, I<handle> will check the validity of the signature and
return indication of the validity of the signature.

The return value is a reference to a hash, which may contain the following
keys, depending on the data passed to the method:

=over 4

=item * Plaintext

If the data is encrypted, the decrypted message.

=item * Validity

If the data is signed, a true value if the signature is valid, a false value
otherwise. The true value will be either the signer's email address, if
available, or C<1>, if not.

=item * Signature

If the data is signed, the I<Crypt::OpenPGP::Signature> object representing
the signature.

=back

If an error occurs, the return value will be C<undef>, and the error message
can be obtained by calling I<errstr> on the I<Crypt::OpenPGP> object.

I<%args> can contain:

=over 4

=item * Data

The data to be "handled". This should be a simple scalar containing an
arbitrary amount of data.

I<Data> is optional; if unspecified, you should specify a filename (see
I<Filename>, below).

=item * Filename

The path to a file to "handle".

I<Filename> is optional; if unspecified, you should specify the data
in I<Data>, above. If both I<Data> and I<Filename> are specified, the
data in I<Data> overrides that in I<Filename>.

=item * PassphraseCallback

If the data is encrypted, you will need to supply I<handle> with the proper
passphrase to unlock the private key, or the password to decrypt the
symmetrically-encrypted data (depending on the method of encryption used).
If you do not specify this parameter, this default passphrase callback will be
used:

    sub _default_passphrase_cb {
        my($cert) = @_;
        my $prompt;
        if ($cert) {
            $prompt = sprintf qq(
    You need a passphrase to unlock the secret key for
    user "%s".
    %d-bit %s key, ID %s
    
    Enter passphrase: ), $cert->uid,
                         $cert->key->size,
                         $cert->key->alg,
                         substr($cert->key_id_hex, -8, 8);
        } else {
            $prompt = "Enter passphrase: ";
        }
        _prompt($prompt, '', 1);
    }

If you do specify this parameter, make sure that your callback function can
handle both asymmetric and symmetric encryption.

See the I<PassphraseCallback> parameter for I<decrypt>, below.

=back

=head2 $pgp->encrypt( %args )

Encrypts a block of data. The encryption is actually done with a symmetric
cipher; the key for the symmetric cipher is then encrypted with either
the public key of the recipient or using a passphrase that you enter. The
former case is using public-key cryptography, the latter, standard
symmetric ciphers. In the first case, the session key can only be
unlocked by someone with the corresponding secret key; in the second, it
can only be unlocked by someone who knows the passphrase.

Given the parameter I<SignKeyID> (see below), I<encrypt> will first sign
the message before encrypting it, adding a Signature packet to the
encrypted plaintext.

Returns a block of data containing two PGP packets: the encrypted
symmetric key and the encrypted data.

On failure returns C<undef>.

I<%args> can contain:

=over 4

=item * Compat

Specifies the PGP compatibility setting. See I<COMPATIBILITY>, above.

=item * Data

The plaintext to be encrypted. This should be a simple scalar containing
an arbitrary amount of data.

I<Data> is optional; if unspecified, you should specify a filename (see
I<Filename>, below).

=item * Filename

The path to a file to encrypt.

I<Filename> is optional; if unspecified, you should specify the data
in I<Data>, above. If both I<Data> and I<Filename> are specified, the
data in I<Data> overrides that in I<Filename>.

=item * Recipients

The intended recipients of the encrypted message. In other words,
either the key IDs or user IDs of the public keys that should be used
to encrypt the message. Each recipient specified should be either a
key ID--an 8-digit or 16-digit hexadecimal number--or part of a user
ID that can be used to look up the user's public key in your keyring.
Examples:

    8-digit hex key ID: 123ABC45
    16-digit hex key ID: 678DEF90123ABC45
    (Part of) User ID: foo@bar

Note that the 8-digit hex key ID is the last 8 digits of the (long)
16-digit hex key ID.

If you wish to encrypt the message for multiple recipients, the value
of I<Recipients> should be a reference to a list of recipients (as
defined above). For each recipient in the list, the public key will be
looked up in your public keyring, and an encrypted session key packet
will be added to the encrypted message.

This argument is optional; if not provided you should provide the
I<Passphrase> option (below) to perform symmetric-key encryption when
encrypting the session key.

=item * KeyID

A deprecated alias for I<Recipients> (above). There is no need to use
I<KeyID>, as its functionality has been completely subsumed into the
I<Recipients> parameter.

=item * Passphrase

The mechanism to use symmetric-key, or "conventional", encryption,
when encrypting the session key. In other words, this allows you to
use I<Crypt::OpenPGP> for encryption/decryption without using public-key
cryptography; this can be useful in certain circumstances (for example,
when encrypting data locally on disk).

This argument is optional; if not provided you should provide the
I<Recipients> option (above) to perform public-key encryption when
encrypting the session key.

=item * RecipientsCallback

After the list of recipients for a message (as given in I<Recipients>,
above) has been mapped into a set of keys from your public keyring,
you can use I<RecipientsCallback> to review/modify that list of keys.
The value of I<RecipientsCallback> should be a reference to a
subroutine; when invoked that routine will be handed a reference to
an array of I<Crypt::OpenPGP::Certificate> objects. It should then
return a reference to a list of such objects.

This can be useful particularly when supplying user IDs in the list
of I<Recipients> for an encrypted message. Since user IDs are looked
up using partial matches (eg. I<b> could match I<b>, I<abc>, I<bar>,
etc.), one intended recipient may actually turn up multiple keys.
You can use I<RecipientsCallback> to audit that list before actually
encrypting the message:

    my %BAD_KEYS = (
        ABCDEF1234567890 => 1,
        1234567890ABCDEF => 1,
    );
    my $cb = sub {
        my $keys = shift;
        my @return;
        for my $cert (@$keys) {
            push @return, $cert unless $BAD_KEYS{ $cert->key_id_hex };
        }
        \@returns;
    };
    my $ct = $pgp->encrypt( ..., RecipientsCallback => $cb, ... );

=item * Cipher

The name of a symmetric cipher with which the plaintext will be
encrypted. Valid arguments are C<DES3>, C<CAST5>, C<Blowfish>, C<IDEA>,
C<Twofish>, C<Rijndael>, C<Rijndael192>, and C<Rijndael256> (the last
two are C<Rijndael> with key sizes of 192 and 256 bits, respectively).

This argument is optional; if you have provided a I<Compat> parameter,
I<Crypt::OpenPGP> will use the appropriate cipher for the supplied
compatibility mode. Otherwise, I<Crypt::OpenPGP> currently defaults to
C<DES3>; this could change in the future.

=item * Compress

The name of a compression algorithm with which the plaintext will be
compressed before it is encrypted. Valid values are C<ZIP> and
C<Zlib>.

By default text is not compressed.

=item * Armour

If true, the data returned from I<encrypt> will be ASCII-armoured. This
can be useful when you need to send data through email, for example.

By default the returned data is not armoured.

=item * SignKeyID

If you wish to sign the plaintext message before encrypting it, provide
I<encrypt> with the I<SignKeyID> parameter and give it a key ID with
which the message can be signed. This allows recipients of your message
to verify its validity.

By default messages not signed.

=item * SignPassphrase

The passphrase to unlock the secret key to be used when signing the
message.

If you are signing the message--that is, if you have provided the
I<SignKeyID> parameter--either this argument or I<SignPassphraseCallback>
is required.

=item * SignPassphraseCallback

The callback routine to enable the passphrase being passed in through
some user-defined routine. See the I<PassphraseCallback> parameter for
I<sign>, below.

If you are signing the message--that is, if you have provided the
I<SignKeyID> parameter--either this argument or I<SignPassphrase> is
required.

=item * MDC

When set to a true value, instructs I<encrypt> to use encrypted MDC
(modification detection code) packets instead of standard encrypted
data packets. These are a newer form of encrypted data packets that
are followed by a C<SHA-1> hash of the plaintext data. This prevents
attacks that modify the encrypted text by using a message digest to
detect changes.

By default I<MDC> is set to C<0>, and I<encrypt> generates standard
encrypted data packets. Set it to a true value to turn on MDC packets.
Note that I<MDC> will automatically be turned on if you are using a
I<Compat> mode that is known to support it.

=back

=head2 $pgp->decrypt( %args )

Decrypts a block of ciphertext. The ciphertext should be of the sort
returned from I<encrypt>, in either armoured or non-armoured form.
This is compatible with all other implementations of PGP: the output
of their encryption should serves as the input to this method.

When called in scalar context, returns the plaintext (that is, the
decrypted ciphertext), or C<undef> on failure. When called in list
context, returns a three-element list containing the plaintext and the
result of signature verification (see next paragraph), or the empty
list on failure. Either of the failure conditions listed here indicates
that decryption failed.

If I<decrypt> is called in list context, and the encrypted text
contains a signature over the plaintext, I<decrypt> will attempt to
verify the signature and will return the result of that verification
as the second element in the return list, and the actual
I<Crypt::OpenPGP::Signature> object as the third element in the return
list. If you call I<decrypt> in
list context and the ciphertext does I<not> contain a signature, that
second element will be C<undef>, and the I<errstr> will be set to
the string C<No Signature\n>. The second element in the return list can
have one of three possible values: C<undef>, meaning that either an
error occurred in verifying the signature, I<or> the ciphertext did
not contain a signature; C<0>, meaning that the signature is invalid;
or a true value of either the signer's user ID or C<1>, if the user ID
cannot be determined. Note that these are the same values returned from
I<verify> (below).

For example, to decrypt a message that may contain a signature that you
want verified, you might use code like this:

    my($pt, $valid, $sig) = $pgp->decrypt( ... );
    die "Decryption failed: ", $pgp->errstr unless $pt;
    die "Signature verification failed: ", $pgp->errstr
        unless defined $valid || $pgp->errstr !~ /^No Signature/;
    print "Signature created at ", $sig->timestamp, "\n";

This checks for errors in decryption, as well as errors in signature
verification, excluding the error denoting that the plaintext was
not signed.

I<%args> can contain:

=over 4

=item * Data

The ciphertext to be decrypted. This should be a simple scalar containing
an arbitrary amount of data.

I<Data> is optional; if unspecified, you should specify a filename (see
I<Filename>, below).

=item * Filename

The path to a file to decrypt.

I<Filename> is optional; if unspecified, you should specify the data
in I<Data>, above. If both I<Data> and I<Filename> are specified, the
data in I<Data> overrides that in I<Filename>.

=item * Passphrase

The passphrase to unlock your secret key, or to decrypt a
symmetrically-encrypted message; the usage depends on how the message is
encrypted.

This argument is optional if your secret key is protected; if not
provided you should supply the I<PassphraseCallback> parameter (below).

=item * PassphraseCallback

A callback routine to allow interactive users (for example) to enter the
passphrase for the specific key being used to decrypt the ciphertext, or
the passphrase used to encrypt a symmetrically-encrypted message. This
is useful when your ciphertext is encrypted to several recipients, if
you do not necessarily know ahead of time the secret key that will be used
to decrypt it. It is also useful when you wish to provide an interactive
user with some feedback about the key being used to decrypt the message,
or when you don't know what type of encryption (symmetric or public-key)
will be used to encrypt a message.

The value of this parameter should be a reference to a subroutine. This
routine will be called when a passphrase is needed from the user, and
it will be given either zero arguments or one argument, depending on
whether the message is encrypted symmetrically (zero arguments) or using
public-key encryption (one argument). If the latter, the one argument is
a I<Crypt::OpenPGP::Certificate> object representing the secret key. You
can use the information in this object to present details about the key to
the user.

In either case, the callback routine should return the passphrase, a
scalar string.

Your callback routine can use the number of arguments to determine how to
prompt the user for a passphrase; for example:

    sub passphrase_cb {
        if (my $cert = $_[0]) {
            printf "Enter passphrase for secret key %s: ",
                $cert->key_id_hex;
        } else {
            print "Enter passphrase: ";
        }
    }

This argument is optional if your secret key is protected; if not
provided you should supply the I<Passphrase> parameter (above).

=back

=head2 $pgp->sign( %args )

Creates and returns a digital signature on a block of data.

On failure returns C<undef>.

I<%args> can contain:

=over 4

=item * Compat

Specifies the PGP compatibility setting. See I<COMPATIBILITY>, above.

=item * Data

The text to be signed. This should be a simple scalar containing an
arbitrary amount of data.

I<Data> is optional; if unspecified, you should specify a filename (see
I<Filename>, below).

=item * Filename

The path to a file to sign.

I<Filename> is optional; if unspecified, you should specify the data
in I<Data>, above. If both I<Data> and I<Filename> are specified, the
data in I<Data> overrides that in I<Filename>.

=item * Detach

If set to a true value the signature created will be a detached
signature; that is, a signature that does not contain the original
text. This assumes that the person who will be verifying the signature
can somehow obtain the original text (for example, if you sign the text
of an email message, the original text is the message).

By default signatures are not detached.

=item * Armour

If true, the data returned from I<sign> will be ASCII-armoured. This
can be useful when you need to send data through email, for example.

By default the returned signature is not armoured.

=item * Clearsign

If true, the signature created on the data is a clear-text signature.
This form of signature displays the clear text of the signed data,
followed by the ASCII-armoured signature on that data. Such a format
is desirable when sending signed messages to groups of users who may
or may not have PGP, because it allows the text of the message to be
readable without special software.

When I<Clearsign> is set to true, I<Armour> and I<Detach> are
automatically turned on, because the signature created is a detached,
armoured signature.

By default I<Clearsign> is false.

=item * KeyID

The ID of the secret key that should be used to sign the message. The
value of the key ID should be specified as a 16-digit hexadecimal number.

This argument is mandatory.

=item * Passphrase

The passphrase to unlock your secret key.

This argument is optional if your secret key is protected; if not
provided you should supply the I<PassphraseCallback> parameter (below).

=item * PassphraseCallback

A callback routine to allow interactive users (for example) to enter the
passphrase for the specific key being used to sign the message. This is
useful when you wish to provide an interactive user with some feedback
about the key being used to sign the message.

The value of this parameter should be a reference to a subroutine. This
routine will be called when a passphrase is needed from the user, and
it will be given one argument: a I<Crypt::OpenPGP::Certificate> object
representing the secret key. You can use the information in this object
to present details about the key to the user. The callback routine
should return the passphrase, a scalar string.

This argument is optional if your secret key is protected; if not
provided you should supply the I<Passphrase> parameter (above).

=item * Digest

The digest algorithm to use when creating the signature; the data to be
signed is hashed by a message digest algorithm, then signed. Possible
values are C<MD5>, C<SHA1>, and C<RIPEMD160>.

This argument is optional; if not provided, the digest algorithm will be
set based on the I<Compat> setting provided to I<sign> or I<new>. If you
have not provided a I<Compat> setting, I<SHA1> will be used.

=item * Version

The format version of the created signature. The two possible values
are C<3> and C<4>; version 4 signatures will not be compatible with
older PGP implementations.

The default value is C<4>, although this could change in the future.

=back

=head2 $pgp->verify( %args )

Verifies a digital signature. Returns true for a valid signature, C<0>
for an invalid signature, and C<undef> if an error occurs (in which
case you should call I<errstr> to determine the source of the error).
The 'true' value returned for a successful signature will be, if available,
the PGP User ID of the person who created the signature. If that
value is unavailable, the return value will be C<1>.

If called in list context, the second element returned in the return list
will be the I<Crypt::OpenPGP::Signature> object representing the actual
signature.

I<%args> can contain:

=over 4

=item * Signature

The signature data, as returned from I<sign>. This data can be either
a detached signature or a non-detached signature. If the former, you
will need to specify the list of files comprising the original signed
data (see I<Data> or I<Files>, below).

Either this argument or I<SigFile> is required.

=item * SigFile

The path to a file containing the signature data. This data can be either
a detached signature or a non-detached signature. If the former, you
will need to specify the list of files comprising the original signed
data (see I<Data> or I<Files>, below).

Either this argument or I<SigFile> is required.

=item * Data

Specifies the original signed data.

If the signature (in either I<Signature> or I<SigFile>) is a detached
signature, either I<Data> or I<Files> is a mandatory argument.

=item * Files

Specifies a list of files comprising the original signed data. The
value should be a reference to a list of file paths; if there is only
one file, the value can be specified as a scalar string, rather than
a reference to a list.

If the signature (in either I<Signature> or I<SigFile>) is a detached
signature, either I<Data> or I<Files> is a mandatory argument.

=back

=head2 $pgp->keygen( %args )

NOTE: this interface is alpha and could change in future releases!

Generates a public/secret PGP keypair. Returns two keyblocks (objects
of type I<Crypt::OpenPGP::KeyBlock>), a public and a secret keyblock,
respectively. A keyblock is essentially a block of keys, subkeys,
signatures, and user ID PGP packets.

I<%args> can contain:

=over 4

=item * Type

The type of key to generate. Currently there are two valid values:
C<RSA> and C<DSA>. C<ElGamal> key generation is not supported at the
moment.

This is a required argument.

=item * Size

Bitsize of the key to be generated. This should be an even integer;
there is no low end currently implemented in I<Crypt::OpenPGP>, but
for the sake of security I<Size> should be at least 1024 bits.

This is a required argument.

=item * Identity

A string that identifies the owner of the key. Typically this is the
combination of the user's name and an email address; for example,

    Foo Bar <foo@bar.com>

The I<Identity> is used to build a User ID packet that is stored in
each of the returned keyblocks.

This is a required argument.

=item * Passphrase

String with which the secret key will be encrypted. When read in from
disk, the key can then only be unlocked using this string.

This is a required argument.

=item * Version

Specifies the key version; defaults to version C<4> keys. You should
only set this to version C<3> if you know why you are doing so (for
backwards compatibility, most likely). Version C<3> keys only support
RSA.

=item * Verbosity

Set to a true value to enable a status display during key generation;
since key generation is a relatively lengthy process, it is helpful
to have an indication that some action is occurring.

I<Verbosity> is 0 by default.

=back

=head1 ERROR HANDLING

If an error occurs in any of the above methods, the method will return
C<undef>. You should then call the method I<errstr> to determine the
source of the error:

    $pgp->errstr

In the case that you do not yet have a I<Crypt::OpenPGP> object (that
is, if an error occurs while creating a I<Crypt::OpenPGP> object),
the error can be obtained as a class method:

    Crypt::OpenPGP->errstr

For example, if you try to decrypt some encrypted text, and you do
not give a passphrase to unlock your secret key:

    my $pt = $pgp->decrypt( Filename => "encrypted_data" )
        or die "Decryption failed: ", $pgp->errstr;

=head1 SAMPLES/TUTORIALS

Take a look at F<bin/pgplet> for an example of usage of I<Crypt::OpenPGP>.
It gives you an example of using the four main major methods (I<encrypt>,
I<sign>, I<decrypt>, and I<verify>), as well as the various parameters to
those methods. It also demonstrates usage of the callback parameters (eg.
I<PassphraseCallback>).

F<bin/pgplet> currently does not have any documentation, but its interface
mirrors that of I<gpg>.

=head1 LICENSE

Crypt::OpenPGP is free software; you may redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR & COPYRIGHT

Except where otherwise noted, Crypt::OpenPGP is Copyright 2001 Benjamin
Trott, cpan@stupidfool.org. All rights reserved.

=head1 REFERENCES

=over 4

=item 1 RFC2440 - OpenPGP Message Format (1998). http://www.faqs.org/rfcs/rfc2440.html

=back 

=cut
