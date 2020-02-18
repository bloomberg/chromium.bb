package Crypt::DSA::GMP::KeyChain;
use strict;
use warnings;

BEGIN {
  $Crypt::DSA::GMP::KeyChain::AUTHORITY = 'cpan:DANAJ';
  $Crypt::DSA::GMP::KeyChain::VERSION = '0.01';
}

use Carp qw( croak );
use Math::BigInt lib => "GMP";
use Math::Prime::Util::GMP qw/is_prob_prime is_provable_prime miller_rabin_random/;
use Digest::SHA qw( sha1 sha1_hex sha256_hex);

use Crypt::DSA::GMP::Key;
use Crypt::DSA::GMP::Util qw( bin2mp bitsize mod_exp makerandomrange randombytes );

sub new {
    my ($class, @params) = @_;
    return bless { @params }, $class;
}

sub generate_params {
    my ($keygen, %param) = @_;
    croak "Size parameter missing" unless defined $param{Size};
    my $bits   = int($param{Size});
    my $v      = $param{Verbosity};
    my $proveq = $param{Prove} && $param{Prove} !~ /^p$/i;
    my $provep = $param{Prove} && $param{Prove} !~ /^q$/i;
    croak "Number of bits (Size => $bits) is too small (min 256)"
      unless $bits >= 256;

    # TODO:
    #  - strict FIPS 186-2 compliance requires L to be a multiple
    #    of 64 512 <= L <= 1024.
    #  - strict FIPS 186-3/4 compliance requires L,N to be one of
    #    the pairs:  (1024,160)  (2048,224)  (2048,256)  (3072,256)
    #  - Can we use new generation method if seed is null?

    # OpenSSL was removed:
    #   1. It was a portability issue (7 RTs related to it).
    #   2. It removes module dependencies.
    #   2. Security issues with running a program in the path without
    #      verifying it is the correct executable.
    #   3. We know the code here follows FIPS 186-4.  OpenSSL does not.
    #   4. The behavior of OpenSSL has changed across different versions.
    #   5. This code is faster for key sizes larger than 1024 bits.

    # Time for key generations (without proofs, average of 1000)
    #  512-bit     47ms Perl      25ms OpenSSL
    #  768-bit     78ms Perl      69ms OpenSSL
    # 1024-bit    139ms Perl     144ms OpenSSL
    # 2048-bit    783ms Perl   1,144ms OpenSSL
    # 4096-bit  7,269ms Perl  12,888ms OpenSSL

    $param{Standard} = $keygen->{Standard}
      if defined $keygen->{Standard} && !defined $param{Standard};
    my $standard = (defined $param{Standard} && $param{Standard} =~ /186-[34]/)
                 ? 'FIPS 186-4'
                 : 'FIPS 186-2';

    # $mrseed is just a random number we give to the primality test to give us
    # a unique sequence of bases.  It's not that important other than (1) we
    # don't want the same sequence each call, (2) we don't want to leak any
    # information about our state, and (3) we don't want to spend too much
    # time/entropy on it.  A truncated hash of our seed should work well.

    my($counter, $q, $p, $seed, $seedp1, $mrseed);

    if ($standard eq 'FIPS 186-2') {

      croak "FIPS 186-2 does not support Q sizes other than 160"
        if defined $param{QSize} && $param{QSize} != 160;
      # See FIPS 186-4 A.1.1.1, non-approved method.
      delete $param{Seed} if defined $param{Seed} && length($param{Seed}) != 20;

      my $n = int(($bits+159)/160)-1;
      my $b = $bits-1-($n*160);
      my $p_test = Math::BigInt->new(2)->bpow($bits-1);   # 2^(L-1)

      do {
        ## Generate q
        while (1) {
          print STDERR "." if $v;
          $seed = (defined $param{Seed}) ? delete $param{Seed}
                                         : randombytes(20);
          $seedp1 = _seed_plus_one($seed);
          my $md = sha1($seed) ^ sha1($seedp1);
          vec($md, 0, 8) |= 0x80;
          vec($md, 19, 8) |= 0x01;
          $q = bin2mp($md);
          $mrseed = '0x'.substr(sha256_hex($seed),0,16) unless defined $mrseed;
          last if ( $proveq && is_provable_prime($q))
               || (!$proveq && is_prob_prime($q)
                            && miller_rabin_random($q, 19, $mrseed));
        }
        print STDERR "*\n" if $v;

        ## Generate p.
        $counter = 0;
        my $q2 = Math::BigInt->new(2)->bmul($q);
        while ($counter < 4096) {
          print STDERR "." if $v;
          my $Wstr = '';
          for my $j (0 .. $n) {
            $seedp1 = _seed_plus_one($seedp1);
            $Wstr = sha1_hex($seedp1) . $Wstr;
          }
          my $W = Math::BigInt->from_hex('0x'.$Wstr)->bmod($p_test);
          my $X = $W + $p_test;
          $p = $X - ( ($X % $q2) - 1);
          if ($p >= $p_test) {
            last if ( $provep && is_provable_prime($p))
                 || (!$provep && is_prob_prime($p)
                              && miller_rabin_random($p, 3, $mrseed));
          }
          $counter++;
        }
      } while ($counter >= 4096);

                # /\ /\ /\ /\   FIPS 186-2   /\ /\ /\ /\ #
    } else {
                # \/ \/ \/ \/   FIPS 186-4   \/ \/ \/ \/ #

      my $L = $bits;
      my $N = (defined $param{QSize}) ? $param{QSize}
                                      : ($bits >= 2048) ? 256 : 160;
      croak "Invalid Q size, must be between 1 and 512" if $N < 1 || $N > 512;
      croak "Invalid Q size, must be >= Size+8" if $L < $N+8;
      # See NIST SP 800-57 rev 3, table 3.  sha256 is ok for all sizes
      my $outlen = ($N <= 256) ? 256 : ($N <= 384) ? 384 : 512;
      my $sha = Digest::SHA->new($outlen);
      croak "No digest available for Q size $N" unless defined $sha;

      my $n = int(($L+$outlen-1)/$outlen)-1;
      my $b = $L-1-($n*$outlen);
      my $p_test = Math::BigInt->new(2)->bpow($L-1);   # 2^(L-1)
      my $q_test = Math::BigInt->new(2)->bpow($N-1);   # 2^(N-1)
      my $seedlen = int( ($N+7)/8 );
      my $nptests = ($L <= 2048) ? 3 : 2;   # See FIPS 186-4 table C.1
      my $nqtests = ($N <= 160) ? 19 : 27;

      delete $param{Seed}
        if defined $param{Seed} && length($param{Seed}) < $seedlen;
      $param{Seed} = substr($param{Seed}, 0, $seedlen) if defined $param{Seed};

      do {
        ## Generate q
        while (1) {
          print STDERR "." if $v;
          $seed = (defined $param{Seed}) ? delete $param{Seed}
                                         : randombytes($seedlen);
          my $digest = $sha->reset->add($seed)->hexdigest;
          my $U = Math::BigInt->from_hex('0x'.$digest)->bmod($q_test);
          $q = $q_test + $U + 1 - $U->is_odd();
          $mrseed = '0x'.substr(sha256_hex($seed),0,16) unless defined $mrseed;
          last if ( $proveq && is_provable_prime($q))
               || (!$proveq && is_prob_prime($q)
                            && miller_rabin_random($q, $nqtests, $mrseed));
        }
        print STDERR "*\n" if $v;
        $seedp1 = $seed;

        ## Generate p.
        $counter = 0;
        my $q2 = Math::BigInt->new(2)->bmul($q);
        while ($counter < 4*$L) {
          print STDERR "." if $v;
          my $Wstr = '';
          for my $j (0 .. $n) {
            $seedp1 = _seed_plus_one($seedp1);
            $Wstr = $sha->reset->add($seedp1)->hexdigest . $Wstr;
          }
          my $W = Math::BigInt->from_hex('0x'.$Wstr)->bmod($p_test);
          my $X = $W + $p_test;
          $p = $X - ( ($X % $q2) - 1);
          if ($p >= $p_test) {
            last if ( $provep && is_provable_prime($p))
                 || (!$provep && is_prob_prime($p)
                              && miller_rabin_random($p, $nptests, $mrseed));
          }
          $counter++;
        }
      } while ($counter >= 4*$L);

    }

    print STDERR "*" if $v;
    my $e = ($p - 1) / $q;
    my $h = Math::BigInt->bone;
    my $g;
    do {
      $g = mod_exp(++$h, $e, $p);
    } while $g == 1;
    print STDERR "\n" if $v;

    my $key = Crypt::DSA::GMP::Key->new;
    $key->p($p);
    $key->q($q);
    $key->g($g);

    return wantarray ? ($key, $counter, "$h", $seed) : $key;
}

# Using FIPS 186-4 B.1.2 approved method.
sub generate_keys {
    my ($keygen, $key, $nonblock) = @_;
    my $q = $key->q;
    # Generate private key 0 < x < q, using best randomness source.
    my $priv_key = makerandomrange( Max => $q-2, KeyGen => !$nonblock ) + 1;
    my $pub_key = mod_exp($key->g, $priv_key, $key->p);
    $key->priv_key($priv_key);
    $key->pub_key($pub_key);
}

sub _seed_plus_one {
    my($s) = @_;
    for (my $i = length($s)-1; $i >= 0; $i--) {
        vec($s, $i, 8)++;
        last unless vec($s, $i, 8) == 0;
    }
    return $s;
}

1;

=pod

=head1 NAME

Crypt::DSA::GMP::KeyChain - DSA key generation system

=head1 SYNOPSIS

    use Crypt::DSA::GMP::KeyChain;
    my $keychain = Crypt::DSA::GMP::KeyChain->new;

    my $key = $keychain->generate_params(
                    Size      => 512,
                    Seed      => $seed,
                    Verbosity => 1,
              );

    $keychain->generate_keys($key);

=head1 DESCRIPTION

L<Crypt::DSA::GMP::KeyChain> is a lower-level interface to key
generation than the L<Crypt::DSA::GMP/keygen> method.
It allows you to separately generate the I<p>, I<q>,
and I<g> key parameters, given an optional starting seed, bit
sizes for I<p> and I<q>, and which standard to follow for
construction.

You can then call I<generate_keys> to generate the public and
private portions of the key.

=head1 USAGE

=head2 $keychain = Crypt::DSA::GMP::KeyChain->new

Constructs and returns a new L<Crypt::DSA::GMP::KeyChain>
object.  At the moment this isn't particularly useful in
itself, other than being the object you need in order to
call the other methods.

The standard to follow may be given in this call, where it
will be used in all methods unless overridden.


=head2 $key = $keychain->generate_params(%arg)

Generates a set of DSA parameters: the I<p>, I<q>, and I<g>
values of the key. This involves finding primes, and as such
it can be a relatively long process.

When invoked in scalar context, returns a new
I<Crypt::DSA::GMP::Key> object.

In list context, returns the new I<Crypt::DSA::GMP::Key> object
along with: the value of the internal counter when a suitable
prime I<p> was found; the value of I<h> when I<g> was derived;
and the value of the seed (a 20-byte or 32-byte string) when
I<q> was found. These values aren't particularly useful in normal
circumstances, but they could be useful.

I<%arg> can contain:

=over 4

=item * Standard

Indicates which standard is to be followed.  By default,
FIPS 186-2 is used, which maintains backward compatibility
with the L<Crypt::DSA> Perl code and old OpenSSL versions.  If
C<FIPS 186-3> or C<FIPS 186-4> is given, then the FIPS 186-4
key generation will be used.

The important changes made:

  - Using SHA-2 rather than SHA-1 for the CSPRNG.  This produces
    better quality random data for prime generation.
  - Allows I<N> to vary between 1 and 512 rather than fixed at 160.
  - The default size for I<N> when not specified is 256 if I<L> is
    2048 or larger, 160 otherwise.
  - In L<Crypt::DSA::GMP>, the signing and verification will use
    SHA-2 256 for signing and verification when I<N> E<lt>= 256,
    and SHA-2 512 otherwise.  The old standard used SHA-1.

where I<N> is the bit size of I<q>, and I<L> is the bit size of I<p>.
These correspond to the I<QSize> and I<Size> arguments.

The recommended primality tests from FIPS 186-4 are always
performed, since they are more stringent than the older standard
and have no negative impact on the result.

=item * Size

The size in bits of the I<p> value to generate.  The minimum
allowable value is 256, and must also be at least 8 bits larger
than the size of I<q> (defaults to 160, see I<QSize>).

For any use where security is a concern, 1024 bits should be
considered a minimum size.  NIST SP800-57 (July 2012) considers
1024 bit DSA using SHA-1 to be deprecated, with 2048 or more bits
using SHA-2 to be acceptable.

This argument is mandatory.

=item * QSize

The size in bits of the I<q> value to generate.  For the default
FIPS 186-2 standard, this must always be 160.  If the FIPS 186-4
standard is used, then this may be in the range 1 to 512 (values
less than 160 are strongly discouraged).

If not specified, I<q> will be 160 bits if either the default
FIPS 186-2 standard is used or if I<Size> is less than 2048.
If FIPS 186-4 is used and I<Size> is 2048 or larger, then I<q>
will be 256.

=item * Seed

A seed with which I<q> generation will begin. If this seed does
not lead to a suitable prime, it will be discarded, and a new
random seed chosen in its place, until a suitable prime can be
found.

A seed that is shorter than the size of I<q> will be
immediately discarded.

This is entirely optional, and if not provided a random seed will
be generated automatically.  Do not use this option unless you
have a specific need for a starting seed.

=item * Verbosity

Should be either 0 or 1. A value of 1 will give you a progress
meter during I<p> and I<q> generation -- this can be useful, since
the process can be relatively long.

The default is 0.

=item * Prove

Should be 0, 1, I<P>, or I<Q>.  If defined and true, then both
the primes for I<p> and I<q> will be proven primes.  Setting to
the string I<P> or I<Q> will result in just that prime being proven.

Using this flag will guarantee the values are prime, which is
valuable if security is extremely important.  The current
implementation constructs random primes using the method
A.1.1.1, then ensures they are prime by constructing and
verifying a primality proof, rather than using a constructive
method such as the Maurer or Shawe-Taylor algorithms.  The
time for proof will depend on the platform and the Size
parameter.  Proving I<q> should take 100 milliseconds or
less, but I<p> can take a very long time if over 1024 bits.

The default is 0, which means the standard FIPS 186-4 probable
prime tests are done.


=back

=head2 $keychain->generate_keys($key)

Generates the public and private portions of the key I<$key>,
a I<Crypt::DSA::GMP::Key> object.

=head1 AUTHOR & COPYRIGHT

See L<Crypt::DSA::GMP> for author, copyright, and license information.

=cut
