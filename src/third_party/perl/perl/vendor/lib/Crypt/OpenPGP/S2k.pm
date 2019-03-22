package Crypt::OpenPGP::S2k;
use strict;

use Crypt::OpenPGP::Buffer;
use Crypt::OpenPGP::Digest;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

use vars qw( %TYPES );
%TYPES = (
    0 => 'Simple',
    1 => 'Salted',
    3 => 'Salt_Iter',
);

sub new {
    my $class = shift;
    my $type = shift;
    $type = $TYPES{ $type } || $type;
    return $class->error("Invalid type of S2k") unless $type;
    my $pkg = join '::', __PACKAGE__, $type;
    my $s2k = bless { }, $pkg;
    $s2k->init(@_);
}

sub parse {
    my $class = shift;
    my($buf) = @_;
    my $id = $buf->get_int8;
    my $type = $TYPES{$id};
    $class->new($type, $buf);
}

sub init { $_[0] }
sub generate {
    my $s2k = shift;
    my($passphrase, $keysize) = @_;
    my($material, $pass) = ('', 0);
    my $hash = $s2k->{hash};
    while (length($material) < $keysize) {
        my $pad = '' . chr(0) x $pass;
        $material .= $s2k->s2k($passphrase, $pad);
        $pass++;
    }
    substr($material, 0, $keysize);
}
sub set_hash {
    my $s2k = shift;
    my($hash_alg) = @_;
    $s2k->{hash} = ref($hash_alg) ? $hash_alg :
        Crypt::OpenPGP::Digest->new($hash_alg);
}

package Crypt::OpenPGP::S2k::Simple;
use base qw( Crypt::OpenPGP::S2k );

use Crypt::OpenPGP::Constants qw( DEFAULT_DIGEST );

sub init {
    my $s2k = shift;
    my($buf) = @_;
    if ($buf) {
        $s2k->{hash_alg} = $buf->get_int8;
    }
    else {
        $s2k->{hash_alg} = DEFAULT_DIGEST;
    }
    if ($s2k->{hash_alg}) {
        $s2k->{hash} = Crypt::OpenPGP::Digest->new($s2k->{hash_alg});
    }
    $s2k;
}

sub s2k { $_[0]->{hash}->hash($_[2] . $_[1]) }

sub save {
    my $s2k = shift;
    my $buf = Crypt::OpenPGP::Buffer->new;
    $buf->put_int8(1);
    $buf->put_int8($s2k->{hash_alg});
    $buf->bytes;
}

package Crypt::OpenPGP::S2k::Salted;
use base qw( Crypt::OpenPGP::S2k );

use Crypt::OpenPGP::Constants qw( DEFAULT_DIGEST );

sub init {
    my $s2k = shift;
    my($buf) = @_;
    if ($buf) {
        $s2k->{hash_alg} = $buf->get_int8;
        $s2k->{salt} = $buf->get_bytes(8);
    }
    else {
        $s2k->{hash_alg} = DEFAULT_DIGEST;
        require Crypt::Random;
        $s2k->{salt} = Crypt::Random::makerandom_octet( Length => 8 );
    }
    if ($s2k->{hash_alg}) {
        $s2k->{hash} = Crypt::OpenPGP::Digest->new($s2k->{hash_alg});
    }
    $s2k;
}

sub s2k { $_[0]->{hash}->hash($_[0]->{salt} . $_[2] . $_[1]) }

sub save {
    my $s2k = shift;
    my $buf = Crypt::OpenPGP::Buffer->new;
    $buf->put_int8(2);
    $buf->put_int8($s2k->{hash_alg});
    $buf->put_bytes($s2k->{salt});
    $buf->bytes;
}

package Crypt::OpenPGP::S2k::Salt_Iter;
use base qw( Crypt::OpenPGP::S2k );

use Crypt::OpenPGP::Constants qw( DEFAULT_DIGEST );

sub init {
    my $s2k = shift;
    my($buf) = @_;
    if ($buf) {
        $s2k->{hash_alg} = $buf->get_int8;
        $s2k->{salt} = $buf->get_bytes(8);
        $s2k->{count} = $buf->get_int8;
    }
    else {
        $s2k->{hash_alg} = DEFAULT_DIGEST;
        require Crypt::Random;
        $s2k->{salt} = Crypt::Random::makerandom_octet( Length => 8 );
        $s2k->{count} = 96;
    }
    if ($s2k->{hash_alg}) {
        $s2k->{hash} = Crypt::OpenPGP::Digest->new($s2k->{hash_alg});
    }
    $s2k;
}

sub s2k {
    my $s2k = shift;
    my($pass, $pad) = @_;
    my $salt = $s2k->{salt};
    my $count = (16 + ($s2k->{count} & 15)) << (($s2k->{count} >> 4) + 6);
    my $len = length($pass) + 8;
    if ($count < $len) {
        $count = $len;
    }
    my $res = $pad;
    while ($count > $len) {
        $res .= $salt . $pass;
        $count -= $len;
    }
    if ($count < 8) {
        $res .= substr($salt, 0, $count);
    } else {
        $res .= $salt;
        $count -= 8;
        $res .= substr($pass, 0, $count);
    }
    $s2k->{hash}->hash($res);
}

sub save {
    my $s2k = shift;
    my $buf = Crypt::OpenPGP::Buffer->new;
    $buf->put_int8(3);
    $buf->put_int8($s2k->{hash_alg});
    $buf->put_bytes($s2k->{salt});
    $buf->put_int8($s2k->{count});
    $buf->bytes;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::S2k - String-to-key generation

=head1 SYNOPSIS

    use Crypt::OpenPGP::S2k;

    # S2k generates an encryption key from a passphrase; in order to
    # understand how large of a key to generate, we need to know which
    # cipher we're using, and what the passphrase is.
    my $cipher = Crypt::OpenPGP::Cipher->new( '...' );
    my $passphrase = 'foo';

    my $s2k = Crypt::OpenPGP::S2k->new( 'Salt_Iter' );
    my $key = $s2k->generate( $passphrase, $cipher->keysize );

    my $serialized = $s2k->save;

=head1 DESCRIPTION

I<Crypt::OpenPGP::S2k> implements string-to-key generation for use in
generating symmetric cipher keys from standard, arbitrary-length
passphrases (like those used to lock secret key files). Since a
passphrase can be of any length, and key material must be a very
specific length, a method is needed to translate the passphrase into
the key. The OpenPGP RFC defines three such methods, each of which
this class implements.

=head1 USAGE

=head2 Crypt::OpenPGP::S2k->new($type)

Creates a new type of S2k-generator of type I<$type>; valid values for
I<$type> are C<Simple>, C<Salted>, and C<Salt_Iter>. These generator
types are described in the OpenPGP RFC section 3.6.

Returns the new S2k-generator object.

=head2 Crypt::OpenPGP::S2k->parse($buffer)

Given a buffer I<$buffer> of type I<Crypt::OpenPGP::Buffer>, determines
the type of S2k from the first octet in the buffer (one of the types
listed above in I<new>), then creates a new object of that type and
initializes the S2k state from the buffer I<$buffer>. Different
initializations occur based on the type of S2k.

Returns the new S2k-generator object.

=head2 $s2k->save

Serializes the S2k object and returns the serialized form; this form
will differ based on the type of S2k.

=head2 $s2k->generate($passphrase, $keysize)

Given a passphrase I<$passphrase>, which should be a string of octets
of arbitrary length, and a keysize I<$keysize>, generates enough key
material to meet the size I<$keysize>, and returns that key material.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
