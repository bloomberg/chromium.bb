package Crypt::OpenPGP::Cipher;
use strict;

use Crypt::OpenPGP::CFB;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

use vars qw( %ALG %ALG_BY_NAME );
%ALG = (
    1 => 'IDEA',
    2 => 'DES3',
    3 => 'CAST5',
    4 => 'Blowfish',
    7 => 'Rijndael',
    8 => 'Rijndael192',
    9 => 'Rijndael256',
    10 => 'Twofish',
);
%ALG_BY_NAME = map { $ALG{$_} => $_ } keys %ALG;

sub new {
    my $class = shift;
    my $alg = shift;
    $alg = $ALG{$alg} || $alg;
    return $class->error("Unsupported cipher algorithm '$alg'")
        unless $alg =~ /^\D/;
    my $pkg = join '::', $class, $alg;
    my $ciph = bless { __alg => $alg,
                       __alg_id => $ALG_BY_NAME{$alg} }, $pkg;
    my $impl_class = $ciph->crypt_class;
    my @classes = ref($impl_class) eq 'ARRAY' ? @$impl_class : ($impl_class);
    for my $c (@classes) {
        eval "use $c;";
        $ciph->{__impl} = $c, last unless $@;
    }
    return $class->error("Error loading cipher implementation for " .
                         "'$alg': no implementations installed.")
        unless $ciph->{__impl};
    $ciph->init(@_);
}

sub init {
    my $ciph = shift;
    my($key, $iv) = @_;
    if ($key) {
        my $class = $ciph->{__impl};
        ## Make temp variable, because Rijndael checks SvPOK, which
        ## doesn't seem to like a value that isn't a variable?
        my $tmp = substr $key, 0, $ciph->keysize;
        my $c = $class->new($tmp);
        $ciph->{cipher} = Crypt::OpenPGP::CFB->new($c, $iv);
    }
    $ciph;
}

sub encrypt { $_[0]->{cipher}->encrypt($_[1]) }
sub decrypt { $_[0]->{cipher}->decrypt($_[1]) }

sub sync { $_[0]->{cipher}->sync }

sub alg { $_[0]->{__alg} }
sub alg_id {
    return $_[0]->{__alg_id} if ref($_[0]);
    $ALG_BY_NAME{$_[1]} || $_[1];
}
sub supported {
    my $class = shift;
    my %s;
    for my $cid (keys %ALG) {
        my $cipher = $class->new($cid);
        $s{$cid} = $cipher->alg if $cipher;
    }
    \%s;
}

package Crypt::OpenPGP::Cipher::IDEA;
use strict;
use base qw( Crypt::OpenPGP::Cipher );

sub init {
    my $ciph = shift;
    my($key, $iv) = @_;
    if ($key) {
        my $c = IDEA->new(substr($key, 0, $ciph->keysize));
        $ciph->{cipher} = Crypt::OpenPGP::CFB->new($c, $iv);
    }
    $ciph;
}

sub crypt_class { 'Crypt::IDEA' }
sub keysize { 16 }
sub blocksize { 8 }

package Crypt::OpenPGP::Cipher::Blowfish;
use strict;
use base qw( Crypt::OpenPGP::Cipher );

sub crypt_class { 'Crypt::Blowfish' }
sub keysize { 16 }
sub blocksize { 8 }

package Crypt::OpenPGP::Cipher::DES3;
use strict;
use base qw( Crypt::OpenPGP::Cipher );

sub crypt_class { 'Crypt::DES_EDE3' }
sub keysize { 24 }
sub blocksize { 8 }

package Crypt::OpenPGP::Cipher::CAST5;
use strict;
use base qw( Crypt::OpenPGP::Cipher );

sub crypt_class { 'Crypt::CAST5_PP' }
sub keysize { 16 }
sub blocksize { 8 }

package Crypt::OpenPGP::Cipher::Twofish;
use strict;
use base qw( Crypt::OpenPGP::Cipher );

sub crypt_class { 'Crypt::Twofish' }
sub keysize { 32 }
sub blocksize { 16 }

package Crypt::OpenPGP::Cipher::Rijndael;
use strict;
use base qw( Crypt::OpenPGP::Cipher );

sub crypt_class { 'Crypt::Rijndael' }
sub keysize { 16 }
sub blocksize { 16 }

package Crypt::OpenPGP::Cipher::Rijndael192;
use strict;
use base qw( Crypt::OpenPGP::Cipher );

sub crypt_class { 'Crypt::Rijndael' }
sub keysize { 24 }
sub blocksize { 16 }

package Crypt::OpenPGP::Cipher::Rijndael256;
use strict;
use base qw( Crypt::OpenPGP::Cipher );

sub crypt_class { 'Crypt::Rijndael' }
sub keysize { 32 }
sub blocksize { 16 }

1;
__END__

=head1 NAME

Crypt::OpenPGP::Cipher - PGP symmetric cipher factory

=head1 SYNOPSIS

    use Crypt::OpenPGP::Cipher;

    my $alg = 'Rijndael';
    my $cipher = Crypt::OpenPGP::Cipher->new( $alg );

    my $plaintext = 'foo bar';
    my $ct = $cipher->encrypt($plaintext);
    my $pt = $cipher->decrypt($ct);

=head1 DESCRIPTION

I<Crypt::OpenPGP::Cipher> is a factory class for PGP symmetric ciphers.
All cipher objects are subclasses of this class and share a common
interface; when creating a new cipher object, the object is blessed
into the subclass to take on algorithm-specific functionality.

A I<Crypt::OpenPGP::Cipher> object is a wrapper around a
I<Crypt::OpenPGP::CFB> object, which in turn wraps around the actual
cipher implementation (eg. I<Crypt::Blowfish> for a Blowfish cipher).
This allows all ciphers to share a common interface and a simple
instantiation method.

=head1 USAGE

=head2 Crypt::OpenPGP::Cipher->new($cipher)

Creates a new symmetric cipher object of type I<$cipher>; I<$cipher>
can be either the name of a cipher (in I<Crypt::OpenPGP> parlance) or
the numeric ID of the cipher (as defined in the OpenPGP RFC). Using
a cipher name is recommended, for the simple reason that it is easier
to understand quickly (not everyone knows the cipher IDs).

Valid cipher names are: C<IDEA>, C<DES3>, C<Blowfish>, C<Rijndael>,
C<Rijndael192>, C<Rijndael256>, C<Twofish>, and C<CAST5>.

Returns the new cipher object on success. On failure returns C<undef>;
the caller should check for failure and call the class method I<errstr>
if a failure occurs. A typical reason this might happen is an
unsupported cipher name or ID.

=head2 $cipher->encrypt($plaintext)

Encrypts the plaintext I<$plaintext> and returns the encrypted text
(ie. ciphertext). The encryption is done in CFB mode using the
underlying cipher implementation.

=head2 $cipher->decrypt($ciphertext)

Decrypts the ciphertext I<$ciphertext> and returns the plaintext. The
decryption is done in CFB mode using the underlying cipher
implementation.

=head2 $cipher->alg

Returns the name of the cipher algorithm (as listed above in I<new>).

=head2 $cipher->alg_id

Returns the numeric ID of the cipher algorithm.

=head2 $cipher->blocksize

Returns the blocksize of the cipher algorithm (in bytes).

=head2 $cipher->keysize

Returns the keysize of the cipher algorithm (in bytes).

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
