package Crypt::OpenPGP::Config;
use strict;

use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub new {
    my $class = shift;
    my $cfg = bless { o => { @_ } }, $class;
    $cfg;
}

sub get { $_[0]->{o}{ $_[1] } }
sub set {
    my $cfg = shift;
    my($key, $val) = @_;
    $cfg->{o}{$key} = $val;
}

{
    my %STANDARD = (
        str => \&_set_str,
        bool => \&_set_bool,
    );

    sub read_config {
        my $cfg = shift;
        my($compat, $cfg_file) = @_;
        my $class = join '::', __PACKAGE__, $compat;
        my $directives = $class->directives;
        local(*FH, $_, $/);
        $/ = "\n";
        open FH, $cfg_file or
            return $cfg->error("Error opening file '$cfg_file': $!");
        while (<FH>) {
            chomp;
            next if !/\S/ || /^#/;
            if (/^\s*([^\s=]+)(?:(?:(?:\s*=\s*)|\s+)(.*))?/) {
                my($key, $val) = ($1, $2);
                my $ref = $directives->{lc $key};
                next unless $ref;
                my $code = ref($ref->[0]) eq 'CODE' ? $ref->[0] :
                                                      $STANDARD{$ref->[0]};
                $code->($cfg, $ref->[1], $val);
            }
        }
        close FH;
    }
}

sub _set_str { $_[0]->{o}{$_[1]} = $_[2] }
{
    my %BOOL = ( off => 0, on => 1 );
    sub _set_bool {
        my($cfg, $key, $val) = @_;
        $val = 1 unless defined $val;
        $val = $BOOL{$val} || $val;
        $cfg->{o}{$key} = $val;
    }
}

package Crypt::OpenPGP::Config::GnuPG;
sub directives {
    {
        armor => [ 'bool', 'Armour' ],
        'default-key' => [ 'str', 'DefaultKey' ],
        recipient => [ 'str', 'Recipient' ],
        'default-recipient' => [ 'str', 'DefaultRecipient' ],
        'default-recipient-self' => [ 'bool', 'DefaultSelfRecipient' ],
        'encrypt-to' => [ 'str', 'Recipient' ],
        verbose => [ 'bool', 'Verbose' ],
        textmode => [ 'bool', 'TextMode' ],
        keyring => [ 'str', 'PubRing' ],
        'secret-keyring' => [ 'str', 'SecRing' ],
        'cipher-algo' => [ \&_set_cipher ],
        'digest-algo' => [ 'str', 'Digest' ],
        'compress-algo' => [ \&_set_compress ],
    }
}

{
    my %Ciphers = (
        '3DES' => 'DES3',             BLOWFISH => 'Blowfish',
        RIJNDAEL => 'Rijndael',       RIJNDAEL192 => 'Rijndael192',
        RIJNDAEL256 => 'Rijndael256', TWOFISH => 'Twofish',
        CAST5 => 'CAST5',
    );
    sub _set_cipher { $_[0]->{o}{Cipher} = $Ciphers{$_[2]} }

    my %Compress = ( 1 => 'ZIP', 2 => 'Zlib' );
    sub _set_compress { $_[0]->{o}{Compress} = $Compress{$_[2]} }
}

package Crypt::OpenPGP::Config::PGP2;
sub directives {
    {
        armor => [ 'bool', 'Armour' ],
        compress => [ 'bool', 'Compress' ],
        encrypttoself => [ 'bool', 'EncryptToSelf' ],
        myname => [ 'str', 'DefaultSelfRecipient' ],
        pubring => [ 'str', 'PubRing' ],
        secring => [ 'str', 'SecRing' ],
    }
}

package Crypt::OpenPGP::Config::PGP5;
*directives = \&Crypt::OpenPGP::Config::PGP2::directives;

1;
