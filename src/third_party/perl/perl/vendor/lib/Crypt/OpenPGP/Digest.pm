package Crypt::OpenPGP::Digest;
use strict;

use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

use vars qw( %ALG %ALG_BY_NAME );
%ALG = (
    1 => 'MD5',
    2 => 'SHA1',
    3 => 'RIPEMD160',
);
%ALG_BY_NAME = map { $ALG{$_} => $_ } keys %ALG;

sub new {
    my $class = shift;
    my $alg = shift;
    $alg = $ALG{$alg} || $alg;
    return $class->error("Unsupported digest algorithm '$alg'")
        unless $alg =~ /^\D/;
    my $pkg = join '::', $class, $alg;
    my $dig = bless { __alg => $alg,
                      __alg_id => $ALG_BY_NAME{$alg} }, $pkg;
    $dig->init(@_);
}

sub init { $_[0] }
sub hash { $_[0]->{md}->($_[1]) }

sub alg {
    return $_[0]->{__alg} if ref($_[0]);
    $ALG{$_[1]} || $_[1];
}

sub alg_id {
    return $_[0]->{__alg_id} if ref($_[0]);
    $ALG_BY_NAME{$_[1]} || $_[1];
}

sub supported {
    my $class = shift;
    my %s;
    for my $did (keys %ALG) {
        my $digest = $class->new($did);
        $s{$did} = $digest->alg if $digest;
    }
    \%s;
}

package Crypt::OpenPGP::Digest::MD5;
use strict;
use base qw( Crypt::OpenPGP::Digest );

sub init {
    my $dig = shift;
    require Digest::MD5;
    $dig->{md} = \&Digest::MD5::md5;
    $dig;
}

package Crypt::OpenPGP::Digest::SHA1;
use strict;
use base qw( Crypt::OpenPGP::Digest );

sub init {
    my $dig = shift;
    require Digest::SHA1;
    $dig->{md} = \&Digest::SHA1::sha1;
    $dig;
}

package Crypt::OpenPGP::Digest::RIPEMD160;
use strict;
use base qw( Crypt::OpenPGP::Digest );

sub init {
    my $dig = shift;
    require Crypt::RIPEMD160;
    $dig->{md} = sub { Crypt::RIPEMD160->hash($_[0]) };
    $dig;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Digest - PGP message digest factory

=head1 SYNOPSIS

    use Crypt::OpenPGP::Digest;

    my $alg = 'SHA1';
    my $dgst = Crypt::OpenPGP::Digest->new( $alg );
    my $data = 'foo bar';
    my $hashed_data = $dgst->hash($data);

=head1 DESCRIPTION

I<Crypt::OpenPGP::Digest> is a factory class for PGP message digest
objects. All digest objects are subclasses of this class and share a
common interface; when creating a new digest object, the object is
blessed into the subclass to take on algorithm-specific functionality.

A I<Crypt::OpenPGP::Digest> object wraps around a function reference
providing the actual digest implementation (eg. I<Digest::MD::md5> for
an MD5 digest). This allows all digest objects to share a common
interface and a simple instantiation method.

=head1 USAGE

=head2 Crypt::OpenPGP::Digest->new($digest)

Creates a new message digest object of type I<$digest>; I<$digest> can
be either the name of a digest algorithm (in I<Crypt::OpenPGP>
parlance) or the numeric ID of the algorithm (as defined in the
OpenPGP RFC). Using an algorithm name is recommended, for the simple
reason that it is easier to understand quickly (not everyone knows
the algorithm IDs).

Valid digest names are: C<MD5>, C<SHA1>, and C<RIPEMD160>.

Returns the new digest object on success. On failure returns C<undef>;
the caller should check for failure and call the class method I<errstr>
if a failure occurs. A typical reason this might happen is an
unsupported digest name or ID.

=head2 $dgst->hash($data)

Creates a message digest hash of the data I<$data>, a string of
octets, and returns the digest.

=head2 $dgst->alg

Returns the name of the digest algorithm (as listed above in I<new>).

=head2 $dgst->alg_id

Returns the numeric ID of the digest algorithm.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
