package Crypt::OpenPGP::Key;
use strict;

use Carp qw( confess );
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

use vars qw( %ALG %ALG_BY_NAME );
%ALG = (
    1 => 'RSA',
    16 => 'ElGamal',
    17 => 'DSA',
);
%ALG_BY_NAME = map { $ALG{$_} => $_ } keys %ALG;

sub new {
    my $class = shift;
    my $alg = shift;
    $alg = $ALG{$alg} || $alg;
    my $pkg = join '::', $class, $alg;
    eval "use $pkg;";
    return $class->error("Unsupported algorithm '$alg': $@") if $@;
    my @valid = $pkg->all_props;
    my %valid = map { $_ => 1 } @valid;
    my $key = bless { __valid => \%valid, __alg => $alg,
                      __alg_id => $ALG_BY_NAME{$alg} }, $pkg;
    $key->init(@_);
}

sub keygen {
    my $class = shift;
    my $alg = shift;
    $alg = $ALG{$alg} || $alg;
    my $pkg = join '::', __PACKAGE__, 'Public', $alg;
    eval "use $pkg;";
    return $class->error("Unsupported algorithm '$alg': $@") if $@;
    my($pub_data, $sec_data) = $pkg->keygen(@_);
    return $class->error("Key generation failed: " . $class->errstr)
        unless $pub_data && $sec_data;
    my $pub_pkg = join '::', __PACKAGE__, 'Public';
    my $pub = $pub_pkg->new($alg, $pub_data);
    my $sec_pkg = join '::', __PACKAGE__, 'Secret';
    my $sec = $sec_pkg->new($alg, $sec_data);
    ($pub, $sec);
}

sub init { $_[0] }

sub check { 1 }

sub alg { $_[0]->{__alg} }
sub alg_id { $_[0]->{__alg_id} }

sub size { 0 }
sub bytesize { int(($_[0]->size + 7) / 8) }

sub public_key { }
sub is_secret { 0 }

sub can_encrypt { 0 }
sub can_sign { 0 }

sub DESTROY { }

use vars qw( $AUTOLOAD );
sub AUTOLOAD {
    my $key = shift;
    (my $meth = $AUTOLOAD) =~ s/.*:://;
    confess "Can't call method $meth on Key $key"
        unless $key->{__valid}{$meth};
    $key->{key_data}->$meth(@_);
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Key - OpenPGP key factory

=head1 SYNOPSIS

    use Crypt::OpenPGP::Key;
    my($pub, $sec) = Crypt::OpenPGP::Key->keygen('DSA', Size => 1024);

    use Crypt::OpenPGP::Key::Public;
    my $pubkey = Crypt::OpenPGP::Key::Public->new('DSA');

    use Crypt::OpenPGP::Key::Secret;
    my $seckey = Crypt::OpenPGP::Key::Secret->new('RSA');

=head1 DESCRIPTION

I<Crypt::OpenPGP::Key> provides base class functionality for all
I<Crypt::OpenPGP> public and secret keys. It functions as a factory
class for key generation and key instantiation.

The only time you will ever use I<Crypt::OpenPGP::Key> directly is
to generate a key-pair; in all other scenarios--for example, when
instantiating a new key object--you should use either
I<Crypt::OpenPGP::Key::Public> or I<Crypt::OpenPGP::Key::Secret>,
depending on whether the key is public or secret, respectively.

=head1 KEY GENERATION

=head2 Crypt::OpenPGP::Key->keygen( $type, %arg )

Generates a new key-pair of public key algorithm I<$type>. Returns
a public and a secret key, each blessed into the appropriate
implementation class. Returns an empty list on failure, in which case
you should call the class method I<errstr> to determine the error.

Valid values for type are C<DSA>, C<RSA>, and C<ElGamal>.

I<%arg> can contain:

=over 4

=item * Size

Bitsize of the key to be generated. This should be an even integer;
there is no low end currently set, but for the sake of security
I<Size> should be at least 1024 bits.

This is a required argument.

=item * Verbosity

Set to a true value to enable a status display during key generation;
since key generation is a relatively length process, it is helpful
to have an indication that some action is occurring.

I<Verbosity> is 0 by default.

=back

=head1 METHODS

I<Crypt::OpenPGP::Key> is not meant to be used directly (unless you
are generating keys; see I<KEY GENERATION>, above); instead you should
use the subclasses of this module. There are, however, useful interface
methods that are shared by all subclasses.

=head2 Key Data Access

Each public-key algorithm has different key data associated with it.
For example, a public DSA key has 4 attributes: I<p>, I<q>, I<g>, and
I<y>. A secret DSA key has the same attributes as a public key, and
in addition it has an attribute I<x>.

All of the key data attributes can be accessed by calling methods of
the same name on the I<Key> object. For example:

    my $q = $dsa_key->q;

The attributes for each public-key algorithm are:

=over 4

=item * RSA

Public key: I<n>, I<e>

Secret key: I<n>, I<e>, I<d>, I<p>, I<q>, I<u>

=item * DSA

Public key: I<p>, I<q>, I<g>, I<y>

Secret key: I<p>, I<q>, I<g>, I<y>, I<x>

=item * ElGamal

Public key: I<p>, I<g>, I<y>

Secret key: I<p>, I<g>, I<y>, I<x>

=back

=head2 $key->check

Check the key data to determine if it is valid. For example, an RSA
secret key would multiply the values of I<p> and I<q> and verify that
the product is equal to the value of I<n>. Returns true if the key
is valid, false otherwise.

Not all public key algorithm implementations implement a I<check>
method; for those that don't, I<check> will always return true.

=head2 $key->size

Returns the "size" of the key. The definition of "size" depends on
the public key algorithm; for example, DSA defines the size of a key
as the bitsize of the value of I<p>.

=head2 $key->bytesize

Whereas I<size> will return a bitsize of the key, I<bytesize> returns
the size in bytes. This value is defined as C<int((bitsize(key)+7)/8)>.

=head2 $key->is_secret

Returns true if the key I<$key> is a secret key, false otherwise.

=head2 $key->public_key

Returns the public part of the key I<$key>. If I<$key> is already a
public key, I<$key> is returned; otherwise a new public key object
(I<Crypt::OpenPGP::Key::Public>) is constructed, and the public values
from the secret key are copied into the public key. The new public
key is returned.

=head2 $key->can_encrypt

Returns true if the key algorithm has encryption/decryption
capabilities, false otherwise.

=head2 $key->can_sign

Returns true if the key algorithm has signing/verification
capabilities, false otherwise.

=head2 $key->alg

Returns the name of the public key algorithm.

=head2 $key->alg_id

Returns the number ID of the public key algorithm.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
