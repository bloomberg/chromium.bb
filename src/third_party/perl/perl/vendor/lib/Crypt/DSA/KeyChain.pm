package Crypt::DSA::KeyChain;

use strict;
use Math::BigInt 1.78 try => 'GMP, Pari';
use Digest::SHA1 qw( sha1 );
use Carp qw( croak );
use IPC::Open3;
use File::Spec;
use File::Which ();
use Symbol qw( gensym );

use vars qw{$VERSION};
BEGIN {
    $VERSION = '1.17';
}

use Crypt::DSA::Key;
use Crypt::DSA::Util qw( bin2mp bitsize mod_exp makerandom isprime );

sub new {
    my $class = shift;
    bless { @_ }, $class;
}

sub generate_params {
    my $keygen = shift;
    my %param  = @_;
    my $bits   = Math::BigInt->new($param{Size});
    croak "Number of bits (Size) is too small" unless $bits;
    delete $param{Seed} if $param{Seed} && length $param{Seed} != 20;
    my $v = $param{Verbosity};

    # try to use fast implementations found on the system, if available.
    unless ($param{Seed} || wantarray || $param{PurePerl}) {

        # OpenSSL support
        my $bin     = $^O eq 'MSWin32' ? 'openssl.exe' : 'openssl';
        my $openssl = File::Which::which($bin);
        if ( $openssl ) {
            print STDERR "Using openssl\n" if $v;
            my $bits_n = int($bits);
            open( NULL, ">", File::Spec->devnull );
            my $pid = open3( gensym, \*OPENSSL, ">&NULL", "$openssl dsaparam -text -noout $bits_n" );
            my @res;
            while( <OPENSSL> ) {
                push @res, $_;
            }
            waitpid( $pid, 0 );
            close OPENSSL;
            close NULL;

            my %parts;
            my $cur_part;
            foreach (@res) {
                if (/^\s+(\w):\s*$/) {
                    $cur_part = lc($1);
                    next;
                }
                if (/^\s*((?:[0-9a-f]{2,2}:?)+)\s*$/) {
                    $parts{$cur_part} .= $1;
                }
            }

            $parts{$_} =~ s/://g for keys %parts;

            if (scalar keys %parts == 3) {
                my $key = Crypt::DSA::Key->new;
                $key->p(Math::BigInt->new("0x" . $parts{p}));
                $key->q(Math::BigInt->new("0x" . $parts{q}));
                $key->g(Math::BigInt->new("0x" . $parts{g}));
                return $key;
            }
        }

    }

    # Pure Perl version:

    my($counter, $q, $p, $seed, $seedp1) = (0);

    ## Generate q.
    SCOPE: {
        print STDERR "." if $v;
        $seed = $param{Seed} ? delete $param{Seed} :
            join '', map chr rand 256, 1..20;
        $seedp1 = _seed_plus_one($seed);
        my $md = sha1($seed) ^ sha1($seedp1);
        vec($md, 0, 8) |= 0x80;
        vec($md, 19, 8) |= 0x01;
        $q = bin2mp($md);
        redo unless isprime($q);
    }

    print STDERR "*\n" if $v;
    my $n = int(("$bits"-1) / 160);
    my $b = ($bits-1)-Math::BigInt->new($n)*160;
    my $p_test = Math::BigInt->new(1); $p_test <<= ($bits-1);

    ## Generate p.
    SCOPE: {
        print STDERR "." if $v;
        my $W = Math::BigInt->new(0);
        for my $k (0..$n) {
            $seedp1 = _seed_plus_one($seedp1);
            my $r0 = bin2mp(sha1($seedp1));
            $r0 %= Math::BigInt->new(2) ** $b
                if $k == $n;
            $W += $r0 << (Math::BigInt->new(160) * $k);
        }
        my $X = $W + $p_test;
        $p = $X - ($X % (2 * $q) - 1);
        last if $p >= $p_test && isprime($p);
        redo unless ++$counter >= 4096;
    }

    print STDERR "*" if $v;
    my $e = ($p - 1) / $q;
    my $h = Math::BigInt->new(2);
    my $g;
    SCOPE: {
        $g = mod_exp($h, $e, $p);
        $h++, redo if $g == 1;
    }
    print STDERR "\n" if $v;

    my $key = Crypt::DSA::Key->new;
    $key->p($p);
    $key->q($q);
    $key->g($g);

    return wantarray ? ($key, $counter, "$h", $seed) : $key;
}

sub generate_keys {
    my $keygen = shift;
    my $key = shift;
    my($priv_key, $pub_key);
    SCOPE: {
        my $i = bitsize($key->q);
        $priv_key = makerandom(Size => $i);
        $priv_key -= $key->q if $priv_key >= $key->q;
        redo if $priv_key == 0;
    }
    $pub_key = mod_exp($key->g, $priv_key, $key->p);
    $key->priv_key($priv_key);
    $key->pub_key($pub_key);
}

sub _seed_plus_one {
    my($s, $i) = ($_[0]);
    for ($i=19; $i>=0; $i--) {
        vec($s, $i, 8)++;
        last unless vec($s, $i, 8) == 0;
    }
    $s;
}

1;

=pod

=head1 NAME

Crypt::DSA::KeyChain - DSA key generation system

=head1 SYNOPSIS

    use Crypt::DSA::KeyChain;
    my $keychain = Crypt::DSA::KeyChain->new;

    my $key = $keychain->generate_params(
                    Size      => 512,
                    Seed      => $seed,
                    Verbosity => 1,
              );

    $keychain->generate_keys($key);

=head1 DESCRIPTION

I<Crypt::DSA::KeyChain> is a lower-level interface to key
generation than the interface in I<Crypt::DSA> (the I<keygen>
method). It allows you to separately generate the I<p>, I<q>,
and I<g> key parameters, given an optional starting seed, and
a mandatory bit size for I<p> (I<q> and I<g> are 160 bits each).

You can then call I<generate_keys> to generate the public and
private portions of the key.

=head1 USAGE

=head2 $keychain = Crypt::DSA::KeyChain->new

Constructs a new I<Crypt::DSA::KeyChain> object. At the moment
this isn't particularly useful in itself, other than being the
object you need in order to call the other methods.

Returns the new object.

=head2 $key = $keychain->generate_params(%arg)

Generates a set of DSA parameters: the I<p>, I<q>, and I<g>
values of the key. This involves finding primes, and as such
it can be a relatively long process.

When invoked in scalar context, returns a new
I<Crypt::DSA::Key> object.

In list context, returns the new I<Crypt::DSA::Key> object,
along with: the value of the internal counter when a suitable
prime I<p> was found; the value of I<h> when I<g> was derived;
and the value of the seed (a 20-byte string) when I<q> was
found. These values aren't particularly useful in normal
circumstances, but they could be useful.

I<%arg> can contain:

=over 4

=item * Size

The size in bits of the I<p> value to generate. The I<q> and
I<g> values are always 160 bits each.

This argument is mandatory.

=item * Seed

A seed with which I<q> generation will begin. If this seed does
not lead to a suitable prime, it will be discarded, and a new
random seed chosen in its place, until a suitable prime can be
found.

This is entirely optional, and if not provided a random seed will
be generated automatically.

=item * Verbosity

Should be either 0 or 1. A value of 1 will give you a progress
meter during I<p> and I<q> generation--this can be useful, since
the process can be relatively long.

The default is 0.

=back

=head2 $keychain->generate_keys($key)

Generates the public and private portions of the key I<$key>,
a I<Crypt::DSA::Key> object.

=head1 AUTHOR & COPYRIGHT

Please see the L<Crypt::DSA> manpage for author, copyright,
and license information.

=cut
