package Crypt::DSA::GMP::Key;
use strict;
use warnings;

BEGIN {
  $Crypt::DSA::GMP::Key::AUTHORITY = 'cpan:DANAJ';
  $Crypt::DSA::GMP::Key::VERSION = '0.01';
}

use Carp qw( croak );
use Math::BigInt lib => "GMP";
use Crypt::DSA::GMP::Util qw( bitsize mod_exp );
use Math::Prime::Util::GMP qw/is_prime/;


sub new {
    my ($class, %param) = @_;
    my $key = bless { }, $class;

    if ($param{Filename} || $param{Content}) {
        if ($param{Filename} && $param{Content}) {
            croak "Filename and Content are mutually exclusive.";
        }
        return $key->read(%param);
    }
    $key->{_validated} = 0;
    $key;
}

sub size {
  my $key = shift;
  return bitsize($key->p);
}
sub sizes {
  my $key = shift;
  return ( bitsize($key->p), bitsize($key->q) );
}

BEGIN {
    no strict 'refs';  ## no critic (ProhibitNoStrict)
    for my $meth (qw( p q g pub_key priv_key )) {
        # Values are stored as Math::BigInt objects
        *$meth = sub {
            my($key, $value) = @_;
            if (defined $value) {
              my $str;
              if (ref($value) eq 'Math::BigInt')  { $key->{$meth} = $value; }
              elsif (ref($value) eq 'Math::Pari') { $str = Math::Pari::pari2pv($value); }
              elsif (ref $value)                  { $str = "$value"; }
              elsif ($value =~ /^0x/)             { $key->{$meth} = Math::BigInt->new($value); }
              else                                { $str = $value; }
              $key->{$meth} = Math::BigInt->new("$str")
                  if defined $str && $str =~ /^\d+$/;
              $key->{_validated} = 0;
            } elsif (@_ > 1 && !defined $value) {
              delete $key->{$meth};
              $key->{_validated} = 0;
            }
            $key->{$meth};
        };
    }
}

# Basic mathematic validation of the key parameters.
sub validate {
  my $key = shift;
  return 0 unless defined $key;
  return 1 if $key->{_validated};
  my ($p, $q, $g, $x) = ($key->p, $key->q, $key->g, $key->priv_key);
  return 0 unless defined $p && defined $q && defined $g;
  return 0 unless is_prime($p) && is_prime($q);
  return 0 unless ($p-1) % $q == 0;
  return 0 unless 1 < $g && $g < $p;
  return 0 unless mod_exp($g, $q, $p)->is_one;

  if (defined $x) {
    return 0 unless 0 < $x && $x < $q;
    my $pub = mod_exp($g, $x, $p);
    if (!defined $key->pub_key) {
      $key->pub_key($pub);
    } else {
      return 0 unless $key->pub_key == $pub;
    }
  }
  my $y = $key->pub_key;
  return 0 unless defined $y;
  return 0 unless $y < $p;
  $key->{_validated} = 1;
  1;
}

# Read and Write turn this base class key into a subclass of the
# appropriate type.  However, how do we map their type string into
# the correct module?
#  1. eval "use $class;"
#     Crypt::DSA does this.  It is really not recommended.
#  2. Use Class::Load
#     Correct dynamic way.
#  3. Hard code
#     Avoids string evals, best security, but not dynamic.

sub read {
    my ($key, %param) = @_;
    my $type = $param{Type} or croak "read: Need a key file 'Type'";
    $key->_subclass_key($type);
    if (my $fname = delete $param{Filename}) {
        open(my $fh, "<", $fname) or return;
        my $blob = do { local $/; <$fh> };
        close $fh or return;
        $param{Content} = $blob;
    }
    $key->deserialize(%param);
}

sub write {
    my ($key, %param) = @_;

    croak "write: Cannot find public key"
      unless defined $key && defined $key->pub_key;

    my $type = $param{Type};
    if (!defined $type) {
      my $pkg = __PACKAGE__;
      ($type) = ref($key) =~ /^${pkg}::(\w+)$/;
    }
    croak "write: Need a key file 'Type'"
      unless defined $type && $type ne '';

    # Subclass key as the requested type.
    $key->_subclass_key($type);

    # Serialize using the subclass method
    my $blob = $key->serialize(%param);

    # Write to file if requested
    if (my $fname = delete $param{Filename}) {
        open(my $fh, ">", $fname) or croak "Can't open $fname: $!";
        print $fh $blob;
        close $fh or croak "Can't close $fname: $!";
    }
    # Return the serialized data
    return $blob;
}

sub _subclass_key {
  my ($key, $type) = @_;
  croak "Key type undefined" unless defined $type;
  if      ($type eq 'PEM') {
                              require Crypt::DSA::GMP::Key::PEM;
                              bless $key, 'Crypt::DSA::GMP::Key::PEM';
  } elsif ($type eq 'SSH2') {
                              require Crypt::DSA::GMP::Key::SSH2;
                              bless $key, 'Crypt::DSA::GMP::Key::SSH2';
  } else {
                              croak "Invalid Key type: '$type'";
  }
  return $key;
}

1;
__END__

=pod

=for stopwords ssh-dss

=head1 NAME

Crypt::DSA::GMP::Key - DSA key

=head1 SYNOPSIS

    use Crypt::DSA::GMP::Key;
    my $key = Crypt::DSA::GMP::Key->new;

    $key->p($p);

=head1 DESCRIPTION

L<Crypt::DSA::GMP::Key> contains a DSA key, both the public and
private portions. Subclasses of L<Crypt::DSA::GMP::Key> implement
I<read> and I<write> methods, such that you can store DSA
keys on disk, and read them back into your application.

=head1 USAGE

Any of the key attributes can be accessed through combination
get/set methods. The key attributes are: I<p>, I<q>, I<g>,
I<priv_key>, and I<pub_key>. For example:

    $key->p($p);
    my $p2 = $key->p;

All the attributes are L<Math::BigInt> objects.  When setting
with a non-Math::BigInt object, we will attempt conversion from
native integers, numeric strings in base 10 or base 16 (the
latter with a C<0x> prefix), Pari objects, and any object that
support stringification to base 10.

=head2 $key = Crypt::DSA::GMP::Key->new(%arg)

Creates a new (empty) key object. All of the attributes are
initialized to 0.

Alternately, if you provide the I<Filename> parameter (see
below), the key will be read from disk.  If you provide
the I<Type> parameter (mandatory if I<Filename> is provided),
be aware that your key will actually be blessed into a subclass
of L<Crypt::DSA::GMP::Key>. Specifically, it will be the class
implementing the specific read functionality for that type,
e.g. L<Crypt::DSA::GMP::Key::PEM>.

Returns the key on success, C<undef> otherwise. (See I<Password>
for one reason why I<new> might return C<undef>).

I<%arg> can contain:

=over 4

=item * Type

The type of file where the key is stored.  Currently the only
types supported are I<PEM> and I<SSH2>.

A PEM file is an optionally encrypted, ASN.1-encoded object.
Support for reading/writing PEM files comes from L<Convert::PEM>.
If you don't have this module installed, the I<new> method will die.

An SSH2 file may either be a public key in I<ssh-dss> format, or
a private key using the SSH2 format.

This argument is mandatory, I<if> you're either reading the file from
disk (i.e. you provide a I<Filename> argument) or you've specified the
I<Content> argument.

=item * Filename

The location of the file which contains the key.
Requires a I<Type> argument so the decoder knows what type of file it
is.  You can't specify I<Content> and I<Filename> at the same time.

=item * Content

The serialized version of the key.  Requires a I<Type> argument so the
decoder knows how to decode it.  You can't specify I<Content> and
I<Filename> at the same time.

=item * Password

If your key file is encrypted, you'll need to supply a
passphrase to decrypt it. You can do that here.

If your passphrase is incorrect, I<new> will return C<undef>.

=back

=head2 $key->write(%arg)

Writes a key (optionally) to disk, using a format that you
define with the I<Type> parameter.

If your I<$key> object has a defined I<priv_key> (private key portion),
the key will be written as a DSA private key object; otherwise, it will
be written out as a public key. Note that not all serialization mechanisms
can produce public keys in this version--currently, only PEM public keys
are supported.

I<%arg> can include:

=over 4

=item * Type

The type of file format that you wish to write, e.g. I<PEM>.

This argument is mandatory, I<unless> your I<$key> object is
already blessed into a subclass (e.g. L<Crypt::DSA::GMP::Key::PEM>),
and you wish to write the file using the same subclass.

=item * Filename

The location of the file on disk where you want the key file
to be written.

=item * Password

If you want the key file to be encrypted, provide this
argument, and the ASN.1-encoded string will be encrypted using
the passphrase as a key.

=back

=head2 $key->read(%arg)

Reads a key (optionally) from disk, using a format that you
define with the I<Type> parameter.

I<%arg> can include:

=over 4

=item * Type

The type of file format, e.g. I<PEM>, in which the key is stored.
This argument is mandatory.

=item * Filename

The location of the file on disk where the key file exists.

=item * Password

If the key file is encrypted, this argument must be provided.

=back


=head1 METHODS

=head2 size

Returns the size of the key in bits, which is the size of the
large prime I<p>.

=head2 sizes

Returns a two entry array (L, N) where L is the bit length of
I<p> and N is the bit length of I<q>.

=head2 validate

Does simple validation on the key and returns 1 if it passes,
and 0 otherwise.  This includes:

=over 4

=item * existence check on I<p>, I<q>, and I<g>

=item * verify primality of I<p> and I<q>

=item * verify I<q> is a factor of I<p-1>

=item * partial validation of I<g> (FIPS 186-4 A.2.2)

=item * existence check of one of I<priv_key> or I<pub_key>

=item * construction or verification of I<pub_key> if I<priv_key> exists

=back

Using the high level L<Crypt::DSA:::GMP> routines, this method
is called after key generation, before signing, and before
verification.  An exception is thrown if the result is not
valid.

=head2 p

The prime modulus I<p>, with bit length L.

=head2 q

A prime divisor of I<p-1>, with bit length N.

=head2 g

A generator of a subgroup of order I<q> in the multiplicative group
of C<GF(p)>.  I<g> is in the range [I<2>,I<p-1>].

=head2 priv_key

The private key that must remain secret.  It is a randomly
generated integer in the range [I<1>,I<q-1>].

=head2 pub_key

The public key, where I<pub_key> = I<g> ^ I<priv_key> mod I<p>.


=head1 AUTHOR & COPYRIGHTS

See L<Crypt::DSA::GMP> for author, copyright, and license information.

=cut
