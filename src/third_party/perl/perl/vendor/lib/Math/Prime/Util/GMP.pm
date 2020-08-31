package Math::Prime::Util::GMP;
use strict;
use warnings;
use Carp qw/croak confess carp/;

BEGIN {
  $Math::Prime::Util::GMP::AUTHORITY = 'cpan:DANAJ';
  $Math::Prime::Util::GMP::VERSION = '0.51';
}

# parent is cleaner, and in the Perl 5.10.1 / 5.12.0 core, but not earlier.
# use parent qw( Exporter );
use base qw( Exporter );
our @EXPORT_OK = qw(
                     is_prime
                     is_prob_prime
                     is_bpsw_prime
                     is_provable_prime
                     is_provable_prime_with_cert
                     is_aks_prime
                     is_nminus1_prime
                     is_nplus1_prime
                     is_bls75_prime
                     is_ecpp_prime
                     is_pseudoprime
                     is_euler_pseudoprime
                     is_euler_plumb_pseudoprime
                     is_strong_pseudoprime
                     is_lucas_pseudoprime
                     is_strong_lucas_pseudoprime
                     is_extra_strong_lucas_pseudoprime
                     is_almost_extra_strong_lucas_pseudoprime
                     is_perrin_pseudoprime
                     is_frobenius_pseudoprime
                     is_frobenius_underwood_pseudoprime
                     is_frobenius_khashin_pseudoprime
                     is_mersenne_prime
                     is_llr_prime
                     is_proth_prime
                     is_miller_prime
                     miller_rabin_random
                     lucas_sequence  lucasu  lucasv
                     primes
                     sieve_primes
                     sieve_twin_primes
                     sieve_prime_cluster
                     sieve_range
                     next_prime
                     prev_prime
                     surround_primes
                     trial_factor
                     prho_factor
                     pbrent_factor
                     pminus1_factor
                     pplus1_factor
                     holf_factor
                     squfof_factor
                     ecm_factor
                     qs_factor
                     factor
                     divisors
                     sigma
                     chinese
                     moebius
                     prime_count prime_count_lower prime_count_upper
                     primorial
                     pn_primorial
                     factorial subfactorial multifactorial factorial_sum
                     factorialmod
                     consecutive_integer_lcm
                     partitions bernfrac bernreal harmfrac harmreal stirling
                     zeta li ei riemannr lambertw
                     addreal subreal mulreal divreal
                     logreal expreal powreal rootreal agmreal
                     gcd lcm kronecker valuation binomial gcdext hammingweight
                     invmod sqrtmod addmod mulmod divmod powmod
                     vecsum vecprod
                     exp_mangoldt
                     liouville
                     totient
                     jordan_totient
                     carmichael_lambda
                     sqrtint rootint logint
                     is_power is_prime_power is_semiprime is_square
                     is_carmichael is_fundamental is_totient
                     is_primitive_root
                     is_polygonal polygonal_nth
                     znorder
                     znprimroot
                     ramanujan_tau
                     Pi Euler
                     todigits
                     random_prime random_nbit_prime random_ndigit_prime
                     random_strong_prime
                     random_maurer_prime random_shawe_taylor_prime
                     random_maurer_prime_with_cert
                     random_shawe_taylor_prime_with_cert
                     seed_csprng is_csprng_well_seeded
                     irand irand64 drand urandomb urandomm urandomr random_bytes
                     permtonum numtoperm
                   );
                   # Should add:
                   # nth_prime
our %EXPORT_TAGS = (all => [ @EXPORT_OK ]);

sub _init_random {
  use Fcntl;
  my($file, $nbytes, $s, $buffer, $nread) = ("/dev/urandom", 256, '', '', 0);
  return if $^O eq 'MSWin32';
  return unless -r $file;
  sysopen(my $fh, $file, O_RDONLY);
  binmode $fh;
  while ($nread < $nbytes) {
    my $thisread = sysread $fh, $buffer, $nbytes-$nread;
    last unless defined $thisread && $thisread > 0;
    $s .= $buffer;
    $nread += length($buffer);
  }
  return unless $nbytes == length($s);
  seed_csprng($nbytes, $s);
}

BEGIN {
  eval {
    require XSLoader;
    XSLoader::load(__PACKAGE__, $Math::Prime::Util::GMP::VERSION);
    _GMP_init();
    _init_random();
    1;
  } or do {
    die $@;
  };
}
END {
  _GMP_destroy();
}


sub _validate_positive_integer {
  my($n, $min, $max) = @_;
  croak "Parameter must be defined" if !defined $n;
  if (ref($n) eq 'Math::BigInt' && $n->can("sign")) {
    croak "Parameter '$n' must be a positive integer" unless $n->sign() eq '+';
  } else {
    my $sn = "$n";
    croak "Parameter '$sn' must be a positive integer"
          if $sn eq '' || $sn =~ tr/0123456789//c;
  }
  croak "Parameter '$n' must be >= $min" if defined $min && $n < $min;
  croak "Parameter '$n' must be <= $max" if defined $max && $n > $max;
  1;
}


sub is_provable_prime {
  my ($n) = @_;
  return 0 if $n < 2;
  return _is_provable_prime($n);
}

sub is_provable_prime_with_cert {
  my ($n) = @_;
  my @composite = (0, '');
  return @composite if $n < 2;

  my ($result, $text) = _is_provable_prime($n, 1);
  return @composite if $result == 0;
  return ($result, '') if $result != 2;
  $text = "Type Small\nN $n\n" if !defined $text || $text eq '';
  $text =~ s/\n$//;
  $text = "[MPU - Primality Certificate]\nVersion 1.0\n\nProof for:\nN $n\n\n$text";
  return ($result, $text);
}

sub primes {
  my($low,$high) = (scalar(@_) == 1) ? (2,$_[0]) : ($_[0], $_[1]);

  _validate_positive_integer($low);
  _validate_positive_integer($high);

  [ sieve_primes($low, $high, 0) ];
}

1;

__END__


# ABSTRACT: Utilities related to prime numbers and factoring, using GMP

=pod

=encoding utf8

=for stopwords Möbius Deléglise Bézout s-gonal gcdext vecsum vecprod moebius totient liouville znorder znprimroot bernfrac bernreal harmfrac harmreal addreal subreal mulreal divreal logreal expreal powreal rootreal agmreal stirling zeta li ei riemannr lambertw lucasu lucasv OpenPFGW gmpy2 nonresidue chinese tuplets sqrtmod addmod mulmod powmod divmod superset sqrtint rootint logint todigits urandomb urandomr

=head1 NAME

Math::Prime::Util::GMP - Utilities related to prime numbers and factoring, using GMP


=head1 VERSION

Version 0.51


=head1 SYNOPSIS

  use Math::Prime::Util::GMP ':all';
  my $n = "115792089237316195423570985008687907853269984665640564039457584007913129639937";

  # This doesn't impact the operation of the module at all, but does let you
  # enter big number arguments directly as well as enter (e.g.): 2**2048 + 1.
  use bigint;

  # These return 0 for composite, 2 for prime, and 1 for probably prime
  # Numbers under 2^64 will return 0 or 2.
  # is_prob_prime does a BPSW primality test for numbers > 2^64
  # is_prime adds some MR tests and a quick test to try to prove the result
  # is_provable_prime will spend a lot of effort on proving primality

  say "$n is probably prime"    if is_prob_prime($n);
  say "$n is ", qw(composite prob_prime def_prime)[is_prime($n)];
  say "$n is definitely prime"  if is_provable_prime($n) == 2;

  # Miller-Rabin and strong Lucas-Selfridge pseudoprime tests
  say "$n is a prime or spsp-2/7/61" if is_strong_pseudoprime($n, 2, 7, 61);
  say "$n is a prime or slpsp"       if is_strong_lucas_pseudoprime($n);
  say "$n is a prime or eslpsp"      if is_extra_strong_lucas_pseudoprime($n);

  # Return array reference to primes in a range.
  my $aref = primes( 10 ** 200, 10 ** 200 + 10000 );

  $next = next_prime($n);    # next prime > n
  $prev = prev_prime($n);    # previous prime < n

  # Primorials and lcm
  say "23# is ", primorial(23);
  say "The product of the first 47 primes is ", pn_primorial(47);
  say "lcm(1..1000) is ", consecutive_integer_lcm(1000);


  # Find prime factors of big numbers
  @factors = factor(5465610891074107968111136514192945634873647594456118359804135903459867604844945580205745718497);

  # Finer control over factoring.
  # These stop after finding one factor or exceeding their limit.
  #                               # optional arguments o1, o2, ...
  @factors = trial_factor($n);    # test up to o1
  @factors = prho_factor($n);     # no more than o1 rounds
  @factors = pbrent_factor($n);   # no more than o1 rounds
  @factors = holf_factor($n);     # no more than o1 rounds
  @factors = squfof_factor($n);   # no more than o1 rounds
  @factors = pminus1_factor($n);  # o1 = smoothness limit, o2 = stage 2 limit
  @factors = ecm_factor($n);      # o1 = B1, o2 = # of curves
  @factors = qs_factor($n);       # (no arguments)

=head1 DESCRIPTION

A module for number theory in Perl using GMP.  This includes primality tests,
getting primes in a range, factoring, and more.

While it certainly can be used directly, the main purpose of this
module is for L<Math::Prime::Util>.  That module will automatically
load this one if it is installed, greatly speeding up many of its
operations on big numbers.

Inputs and outputs for big numbers are via strings, so you do not need
to use a bigint package in your program.  However if you do use bigints,
inputs will be converted internally so there is no need to convert
before a call.  Output results are returned as either Perl scalars
(for native-size) or strings (for bigints).  L<Math::Prime::Util> tries
to reconvert all strings back into the callers bigint type if possible,
which makes it more convenient for calculations.

The various C<is_*_pseudoprime> tests are more appropriately called
C<is_*_probable_prime> or C<is_*_prp>.  They return 1 if the input is a
probable prime based on their test.  The naming convention is historical
and follows Pari, L<Math::Primality>, and some other math packages.
The modern definition of pseudoprime is a I<composite> that passes the
test, rather than any number.


=head1 FUNCTIONS

=head2 is_prob_prime

  my $prob_prime = is_prob_prime($n);
  # Returns 0 (composite), 2 (prime), or 1 (probably prime)

Takes a positive number as input and returns back either 0 (composite),
2 (definitely prime), or 1 (probably prime).

For inputs below C<2^64> the test is deterministic, so the possible
return values are 0 (composite) or 2 (definitely prime).

For inputs above C<2^64>, a probabilistic test is performed.  Only 0
(composite) and 1 (probably prime) are returned.  The current
implementation uses the Baillie-PSW (BPSW) test.  There is a
possibility that composites may be returned marked prime, but since
the test was published in 1980, not a single BPSW pseudoprime has
been found, so it is extremely likely to be prime.
While we believe (Pomerance 1984) that an infinite number of
counterexamples exist, there is a weak conjecture (Martin) that
none exist under 10000 digits.

In more detail, we are using the extra-strong Lucas test
(Grantham 2000) using the Baillie parameter selection method
(see OEIS A217719).  Previous versions of this module used the
strong Lucas test with Selfridge parameters, but the extra-strong
version produces fewer pseudoprimes while running 1.2 - 1.5x faster.
It is slightly stronger than the test used in
L<Pari|http://pari.math.u-bordeaux.fr/faq.html#primetest>.


=head2 is_prime

  say "$n is prime!" if is_prime($n);

Takes a positive number as input and returns back either 0 (composite),
2 (definitely prime), or 1 (probably prime).  Composites will act
exactly like C<is_prob_prime>, as will numbers less than C<2^64>.
For numbers larger than C<2^64>, some additional tests are performed
on probable primes to see if they can be proven by another means.

This call walks the line between the performance of L</is_prob_prime>
and the certainty of L</is_provable_prime>.  Those calls may be more
appropriate in some cases.  What this function does is give most of
the performance of the former, while adding more certainty.  For finer
tuning of this tradeoff, you may instead use L</is_prob_prime> followed
by additional probable prime tests such as L</miller_rabin_random>
and/or L</is_frobenius_underwood_pseudoprime>.

As with L</is_prob_prime>, a BPSW test is first performed.  This is
deterministic for all 64-bit numbers.  Next, if the number is a
Proth or LLR form, then a proof is constructed.  If the result is
still "probably prime" and the input is smaller than the
Sorenson/Webster (2015) deterministic Miller-Rabin limit
(approximately 82 bits) then the 11 or 12 Miller-Rabin tests are
performed and the result is confirmed.  For larger inputs that are
still "probably prime" but under 200 bits, a quick
BLS75 C<n-1> primality proof is attempted.  This is tuned to give up
if the result cannot be quickly determined, and results in success
rates of ~80% at 80 bits, ~30% at 128 bits, and ~13% at 160 bits.
Lastly, for results still "probably prime", an additional random-base
Miller-Rabin test is performed.

The result is that many numbers will return 2 (definitely prime),
and the numbers that return 1 (probably prime) have gone through
more tests than L</is_prob_prime> while not taking too long.

For cryptographic key generation, you may want even more testing for
probable primes (NIST recommends a few more additional M-R tests than
we perform).  The function L</miller_rabin_random> is made for this.
Alternately, a different test such as
L</is_frobenius_underwood_pseudoprime> can be used.
Even better, use L</is_provable_prime> which should be reasonably
fast for sizes under 2048 bits.
Typically for key generation one wants random primes, and there are
many functions for that.


=head2 is_provable_prime

  say "$n is definitely prime!" if is_provable_prime($n) == 2;

Takes a positive number as input and returns back either 0 (composite),
2 (definitely prime), or 1 (probably prime).  A great deal of effort is
taken to return either 0 or 2 for all numbers.

The current method first uses BPSW to find composites and provide a
deterministic answer for tiny numbers (under C<2^64>).  If no
certificate is required, LLR and Proth tests can be run, and small
numbers (under approximately C<2^82>) can be satisfied with a
deterministic Miller-Rabin test.  If the result is still not determined,
a quick  BLS75 C<n-1> test is attempted, followed by ECPP.

The time required for primes of different input sizes on a circa-2009
workstation averages about C<3ms> for 30-digits, C<5ms> for 40-digit,
C<20ms> for 60-digit, C<50ms> for 80-digit, C<100ms> for 100-digit,
C<2s> for 200-digit, and 400-digit inputs about a minute.
Expect a lot of time variation for larger inputs.  You can see progress
indication if verbose is turned on (some at level 1, and a lot at level 2).

A certificate can be obtained along with the result using the
L</is_provable_prime_with_cert> method.  There is no appreciable extra
performance cost for returning a certificate.


=head2 is_provable_prime_with_cert

Takes a positive number as input and returns back an array with two
elements.  The result will be one of:

  (0, '')      The input is composite.

  (1, '')      The input is probably prime but we could not prove it.
               This is a failure in our ability to factor some necessary
               element in a reasonable time, not a significant proof
               failure (in other words, it remains a probable prime).

  (2, '...')   The input is prime, and the certificate contains all the
               information necessary to verify this.

The certificate is a text representation containing all the necessary
information to verify the primality of the input in a reasonable time.
The result can be used with L<Math::Prime::Util/verify_prime> for
verification.  Proof types used include:

  ECPP
  BLS3
  BLS15
  BLS5
  Small

=head2 is_pseudoprime

Takes a positive number C<n> and one or more non-zero positive bases as input.
Returns C<1> if the input is a probable prime to each base, C<0> if not.
This is the simple Fermat primality test.
Removing primes, given base 2 this produces the sequence L<OEIS A001567|http://oeis.org/A001567>.

=head2 is_euler_pseudoprime

Takes a positive number C<n> and one or more non-zero positive bases as input.
Returns C<1> if the input is an Euler probable prime to each base, C<0> if not.
This is the Euler test, sometimes called the Euler-Jacobi test.
Removing primes, given base 2 this produces the sequence L<OEIS A047713|http://oeis.org/A047713>.

=head2 is_strong_pseudoprime

  my $maybe_prime = is_strong_pseudoprime($n, 2);
  my $probably_prime = is_strong_pseudoprime($n, 2, 3, 5, 7, 11, 13, 17);

Takes a positive number C<n> and one or more non-zero positive bases as input.
Returns C<1> if the input is a strong probable prime to each base, C<0> if not.
This is often called the Miller-Rabin test.

If 0 is returned, then the number really is a composite.  If 1 is
returned, then it is either a prime or a strong pseudoprime to all
the given bases.  Given enough distinct bases, the chances become
very strong that the number is actually prime.

Both the input number and the bases may be big integers.  If base
modulo n E<lt>= 1 or base modulo n = n-1, then the result will be 1.
This allows the bases to be larger than n if desired, while still
returning meaningful results.  For example,

  is_strong_pseudoprime(367, 1101)

would incorrectly return 0 if this was not done properly.  A 0 result
should be returned only if n is composite, regardless of the base.

This is usually used in combination with other tests to make either stronger
tests (e.g. the strong BPSW test) or deterministic results for numbers less
than some verified limit (e.g. Jaeschke showed in 1993 that no more than three
selected bases are required to give correct primality test results for any
32-bit number).  Given the small chances of passing multiple bases, there
are some math packages that just use multiple MR tests for primality testing,
though in the early 1990s almost all serious software switched to the
BPSW test.

Even numbers other than 2 will always return 0 (composite).  While the
algorithm works with even input, most sources define it only on odd input.
Returning composite for all non-2 even input makes the function match most
other implementations including L<Math::Primality>'s C<is_strong_pseudoprime>
function.

=head2 miller_rabin_random

  my $maybe_prime = miller_rabin_random($n, 10); # 10 random bases

Takes a positive number (C<n>) as input and a positive number (C<k>) of bases
to use.  Performs C<k> Miller-Rabin tests using uniform random bases
between 2 and C<n-2>.  This is the correct way to perform C<k> Miller-Rabin
tests, rather than the common but broken method of using the first C<k>
primes.

An optional third argument may be given, which is a seed to use.  The seed
should be a number either in decimal, binary with a leading C<0b>, hex with
a leading C<0x>, or octal with a leading C<0>.  It will be converted to a
GMP integer, so may be large.  Typically this is not necessary, but
cryptographic applications may prefer the ability to use this, and it
allows repeatable test results.

There is no check for duplicate bases.  Input sizes below 65 bits make
little sense for this function since L<is_prob_prime> is deterministic
at that size.  For numbers of 65+ bits, the chance of duplicate bases
is quite small.  The exponentiation approximation for the birthday
problem gives a probability of less than 2e-16 for 100 random bases to have
a duplicate with a 65-bit input, and less than 2e-35 with a 128-bit input.


=head2 is_lucas_pseudoprime

=head2 is_strong_lucas_pseudoprime

Takes a positive number as input, and returns 1 if the input is a standard
or strong Lucas probable prime.  The Selfridge method of choosing D, P, and
Q are used (some sources call this a Lucas-Selfridge test).  This is one
half of the BPSW primality test (the Miller-Rabin strong probable prime test
with base 2 being the other half).  The canonical BPSW test (page 1401 of
Baillie and Wagstaff (1980)) uses the strong Lucas test with Selfridge
parameters, but in practice a variety of Lucas tests with different
parameters are used by tests calling themselves BPSW.

The standard Lucas test implemented here corresponds to the Lucas test
described in FIPS 186-4 section C.3.3, though uses a slightly more
efficient calculation.  Since the standard Lucas-Selfridge test is a
subset of the strong Lucas-Selfridge test, I recommend using the strong
test rather than the standard test for cryptographic purposes.  It is
often slightly faster, has over 4x fewer pseudoprimes, and is the method
recommended by Baillie and Wagstaff in their 1980 paper.


=head2 is_extra_strong_lucas_pseudoprime

Takes a positive number as input, and returns 1 if the input is an
extra-strong Lucas probable prime.  This is defined in Grantham (2000),
and is a slightly more stringent test than the strong Lucas test, though
because different parameters are used the pseudoprimes are not a subset.
As expected by the extra conditions, the number of pseudoprimes is less
than 2/3 that of the strong Lucas-Selfridge test.
Runtime performance is 1.2 to 1.5x faster than the strong Lucas test.

The parameters are selected using the Baillie-OEIS method:

  P = 3;
  Q = 1;
  while ( jacobi( P*P-4, n ) != -1 )
    P += 1;


=head2 is_almost_extra_strong_lucas_pseudoprime

Takes a positive number as input and returns 1 if the input is an "almost"
extra-strong Lucas probable prime.  This is the classic extra-strong Lucas
test but without calculating the U sequence.  This makes it very fast,
although as the input increases in size the time converges to the conventional
extra-strong implementation:  at 30 digits this routine is about 15% faster,
at 300 digits it is only 2% faster.

With the current implementations, there is little reason to prefer this unless
trying to reproduce specific results.  The extra-strong implementation has been
optimized to use similar features, removing most of the performance advantage.

An optional second argument (must be between 1 and 256) indicates the
increment amount for P parameter selection.  The default value of one yields
the method described in L</is_extra_strong_lucas_pseudoprime>.  A value of
2 yields the method used in
L<Pari|http://pari.math.u-bordeaux.fr/faq.html#primetest>.

Because the C<U = 0> condition is ignored, this produces about 5% more
pseudoprimes than the extra-strong Lucas test.  However this is still only
66% of the number produced by the strong Lucas-Selfridge test.  No BPSW
counterexamples have been found with any of the Lucas tests described.


=head2 is_euler_plumb_pseudoprime

Takes a positive number C<n> as input and returns 1 if C<n> passes
Colin Plumb's Euler Criterion primality test.  Pseudoprimes to this test
are a subset of the base 2 Fermat and Euler tests, but a superset
of the base 2 strong pseudoprime (Miller-Rabin) test.

The main reason for this test is that is a bit more efficient
than other probable prime tests.


=head2 is_perrin_pseudoprime

Takes a positive number C<n> as input and returns 1 if C<n> divides C<P(n)>
where C<P(n)> is the Perrin number of C<n>.  The Perrin sequence is defined by
C<P(n) = P(n-2) + P(n-3)> with C<P(0) = 3, P(1) = 0, P(2) = 2>.

This is not a commonly used test, as it runs slower than most
of the other probable prime tests and offers little benefit, especially over
combined tests like L</is_bpsw_prime>,
L</is_frobenius_underwood_pseudoprime>, and
L</is_frobenius_khashin_pseudoprime>.

An optional second argument C<r> indicates whether to run additional tests.
With C<r=1>, C<P(-n) = -1 mod n> is also verified, creating the
"minimal restricted" test.
With C<r=2>, the full signature is also tested using the Adams and Shanks (1982)
rules (without the quadratic form test).
With C<r=3>, the full signature is tested using the Grantham (2000) test, which
additionally does not allow pseudoprimes to be divisible by 2 or 23.
The minimal restricted pseudoprime sequence is L<OEIS A018187|http://oeis.org/A018187>.


=head2 is_frobenius_pseudoprime

Takes a positive number C<n> as input, and two optional parameters C<a> and
C<b>, and returns 1 if the C<n> is a Frobenius probable prime with respect
to the polynomial C<x^2 - ax + b>.  Without the parameters, C<b = 2> and
C<a> is the least positive odd number such that C<(a^2-4b|n) = -1>.
This selection has no pseudoprimes below C<2^64> and none known.  In any
case, the discriminant C<a^2-4b> must not be a perfect square.

=head2 is_frobenius_underwood_pseudoprime

Takes a positive number as input, and returns 1 if the input passes the
efficient Frobenius test of Paul Underwood.  This selects a parameter C<a>
as the least non-negative integer such that C<(a^2-4|n)=-1>, then verifies that
C<(x+2)^(n+1) = 2a + 5 mod (x^2-ax+1,n)>.  This combines a Fermat and Lucas
test at a computational cost of about 2.5x a strong pseudoprime test.  This
makes it similar to, but faster than, a standard Frobenius test.

This test is deterministic (no randomness is used).  There are no known
pseudoprimes to this test.  This test also has no overlap with the BPSW
test, making it a very effective method for adding additional certainty.

=head2 is_frobenius_khashin_pseudoprime

Takes a positive number as input, and returns 1 if the input passes the
Frobenius test of Sergey Khashin.  This ensures C<n> is not a perfect square,
selects the parameter C<c> as the smallest odd prime such that C<(c|n)=-1>,
then verifies that C<(1+D)^n = (1-D) mod n> where C<D = sqrt(c) mod n>.

This test is deterministic (no randomness is used).
There are no known pseudoprimes to this test.

=head2 is_bpsw_prime

Given a positive number input, returns 0 (composite), 2 (definitely prime),
or 1 (probably prime), using the BPSW primality test (extra-strong variant).

This function does the extra-strong BPSW test and nothing more.  That is,
it will skip all pretests and any extra work that the L</is_prob_prime>
test may add.  This saves some time if the input has no small factors, such
as testing results that have been sieved.


=head2 is_aks_prime

  say "$n is definitely prime" if is_aks_prime($n);

Takes a positive number as input, and returns 1 if the input passes the
Agrawal-Kayal-Saxena (AKS) primality test.  This is a deterministic
unconditional primality test which runs in polynomial time for general input.

The particular method used is theorem 4.1 from Bernstein (2003).  This is
substantially faster than the original AKS publication, the later version
with improvements by Lenstra (sometimes called the V6 paper), or the later
improvements of Voloch and Bornemann.  It is, by a large order, faster than
any other known implementation as of early 2017.

For theoretical analysis of the primality task, AKS is extremely important.
In practice, it is essentially useless.  Estimated run time for a 150 digit
input is over 2 days, making the case that while the algorithmic complexity
I<growth> is polynomial, the constants are extremely high.  It will take
years for for numbers that ECPP or APR-CL can prove in seconds.

With the C<verbose> option set to 1, the chosen C<r> and C<s> values are
printed before the test starts.  With C<verbose> set to 2 or higher, each
of the C<s> tests results in a C<.> output as the test runs, allowing progress
to be monitored.

Typically you should use L</is_provable_prime> and let it decide the method.

=head2 is_mersenne_prime

  say "2^607-1 (M607) is a Mersenne prime" if is_mersenne_prime(607);

Takes a positive number C<p> as input and returns 1 if C<2^p-1> is prime.
After some pre-testing, the Lucas-Lehmer test is performed.
This is a deterministic unconditional test that runs very fast compared
to other primality methods for numbers of comparable size, and vastly
faster than any known general-form primality proof methods.

=head2 is_llr_prime

Takes a positive number C<n> as input and returns one of: 0 (definitely
composite), 2 (definitely prime), or -1 (test does not indicate anything).
This implements the Lucas-Lehmer-Riesel test for
fast deterministic primality testing on numbers of the form C<k * 2^n - 1>.
If the input is not of this form or if C<k E<gt>= 2^n> then C<-1> will be
returned as the test does not apply.
If C<k = 1> then this is a Mersenne number and the Lucas-Lehmer test is used.
Otherwise, the LLR test is performed.  While not as fast as the
Lucas-Lehmer test for Mersenne numbers, it is almost as fast as a single
strong pseudoprime test (i.e.  Miller-Rabin test) while giving a certain answer.

=head2 is_proth_prime

Takes a positive number C<n> as input and returns one of: 0 (definitely
composite), 2 (definitely prime), or -1 (test does not indicate anything).
This applies Proth's theorem for fast Las Vegas
primality testing on numbers of the form C<k * 2^n + 1>.
If the input is not of this form or if C<k E<gt>= 2^n> then C<-1> will be
returned as the test does not apply.
Otherwise, a search is performed to find a quadratic nonresidue modulo C<n>.
If none can be found after a brief search, C<-1> is returned as no conclusion
can be reached.  Otherwise, Proth's theorem is checked which conclusively
indicates primality.
While not as fast as the Lucas-Lehmer test for Mersenne
numbers, it is almost as fast as a single strong pseudoprime test (i.e.
Miller-Rabin test) while giving a certain answer.

=head2 is_miller_prime

  say "$n is definitely prime" if is_miller_prime($n);
  say "$n is definitely prime assuming the GRH" if is_miller_prime($n, 1);

Takes a positive number as input, and returns 1 if the input passes the
deterministic Miller test.  An optional second argument indicates whether
the Generalized Riemann Hypothesis should be assumed, and defaults to 0.
Setting the verbose flag to 2 or higher will show how many bases are used.
The unconditional test is exponential time, while the conditional test
(assuming the GRH) is polynomial time.

This is a very slow method in practice, and generally should not be used.
The asymptotic complexity of the GRH version is good in theory, matching
ECPP, but in practice it is much slower.  The number of bases used by
the unconditional test grows quite rapidly, impractically many past about
160 bits, and overflows a 64-bit integer at 456 bits -- sizes that are
trivial for the unconditional APR-CL and ECPP tests.

=head2 is_nminus1_prime

  say "$n is definitely prime" if is_nminus1_prime($n);

Takes a positive number as input, and returns 1 if the input passes either
theorem 5 or theorem 7 of the Brillhart-Lehmer-Selfridge primality test.
This is a deterministic unconditional primality test which requires factoring
C<n-1> to a linear factor less than the cube root of the input.  For small
inputs (under 40 digits) this is typically very easy, and some numbers will
naturally lead to this being very fast.  As the input grows, this method
slows down rapidly.

This method is most appropriate for numbers of the form C<k+1> where C<k>
can be easily factored.
Typically you should use L</is_provable_prime> and let it decide the method.

=head2 is_nplus1_prime

Takes a positive number as input, and returns 1 if the input passes either
theorem 17 or theorem 19 of the Brillhart-Lehmer-Selfridge primality test.
This is a deterministic unconditional primality test which requires factoring
C<n+1> to a linear factor less than the cube root of the input.  For small
inputs (under 40 digits) this is typically very easy, and some numbers will
naturally lead to this being very fast.  As the input grows, this method
slows down rapidly.

Disregarding factoring, this is slightly slower than the C<n-1> methods.
It is most appropriate for numbers of the form C<k-1> where C<k> can be
easily factored.

=head2 is_bls75_prime

Takes a positive number as input, and returns 1 if the input passes one of
the tests from the Brillhart-Lehmer-Selfridge (1975) paper.  These use
partial factoring of C<n-1> and C<n+1>.  Currently the implementation will
use one of:

  N-1   Corollary 1, Theorem 5, Theorem 7
  N+1   Corollary 8, Theorem 17, Theorem 19
  Comb  Theorem 20

This is appropriate for cases where either C<n-1> or C<n+1> can be easily
factored, or when both of them have many small factors.

=head2 is_ecpp_prime

  say "$n is definitely prime" if is_ecpp_prime($n);

Takes a positive number as input, and returns 1 if the input passes the
ECPP primality test.  This is the Atkin-Morain Elliptic Curve Primality
Proving algorithm.  It is the fastest primality proving method in
Math::Prime::Util.

This implementation uses a "factor all strategy" (FAS) with backtracking.
A limited set of about 500 precalculated discriminants are used, which works
well for inputs up to 300 digits, and for many inputs up to one thousand
digits.  Having a larger set will help with large numbers (a set of 2650
is available on github in the C<xt/> directory).  A future implementation
may include code to generate class polynomials as needed.

Typically you should use L</is_provable_prime> and let it decide the method.


=head2 primes

  my $aref1 = primes( 1_000_000 );
  my $aref2 = primes( 2 ** 448, 2 ** 448 + 10000 );
  say join ",", @{primes( 2**2048, 2**2048 + 10000 )};

Returns all the primes between the lower and upper limits (inclusive), with
a lower limit of C<2> if none is given.

An array reference is returned, matching the signature of the function
of the same name in L<Math::Prime::Util>.

Values above 64-bit are extra-strong BPSW probable primes.


=head2 prime_count

Returns the number of primes between 2 and C<n> (single argument)
or C<lo> and C<hi> given two arguments.  The values are inclusive.

The method is simple sieving followed by primality testing.  This is
appropriate for small ranges and is useful for very large arguments.
The L<Math::Prime::Util> module has much more sophisticated methods
for 64-bit arguments.


=head2 prime_count_lower

=head2 prime_count_upper

Returns lower or upper bounds for the prime count of the input C<n>.

Bounds use Dusart 2010, Büthe 2014, Büthe 2015, and Axler 2017.


=head2 sieve_primes

  my @primes = sieve_primes(2**100, 2**100 + 10000);
  my @candidates = sieve_primes(2**1000, 2**1000 + 10000, 40000);

Given two arguments C<low> and C<high>, this returns the primes in the
interval (inclusive) as a list.  It operates similar to L<primes>, though
must always have an lower and upper bound and returns a list.

With three arguments C<low>, C<high>, and C<limit>, this does a partial
sieve over the inclusive range and returns the list that pass the sieve.
If C<limit> is less than C<2> then it is identical to the two-argument
version, in that a primality test will be performed after sieving.
Otherwise, sieving is performed up to C<limit>.

The two-argument version is typically only used internally and adds little
functionality.  The three-argument version is quite useful for applications
that want to apply their own primality or other tests, and wish to have a
list of values in the range with no small factors.  This is quite common
for applications involving prime gaps.

Also see L</sieve_range>.


=head2 sieve_range

  my @candidates = sieve_range(2**1000, 10000, 40000);

Given a start value C<n>, and native unsigned integers C<width> and C<depth>,
a sieve of maximum depth C<depth> is done for the C<width> consecutive
numbers beginning with C<n>.  An array of offsets from the start is returned.

The returned list contains those offsets in the range C<n> to C<n+width-1>
where C<n + offset> has no prime factors less than C<depth>.

This function is very similar to the three argument form of L</sieve_primes>.
The differences are using C<(n,width)> instead of C<(low,high)>, and most
importantly returning small offsets from the start value rather than the
values themselves.  This can substantially reduce overhead for
multi-thousand digit numbers.


=head2 sieve_twin_primes

  my @primes = sieve_twin_primes(2**1000, 2**1000 + 500000);

Given two arguments C<low> and C<high>, this returns each lower twin prime
in the interval (inclusive).  The result is a list, not a reference.

This does a partial sieve of the range, removes any non-twin candidates,
then checks that each pair are both BPSW probable primes.  This is
substantially more efficient than sieving for all primes followed by
removing those that are not twin primes.


=head2 sieve_prime_cluster

  # Find some prime septuplets
  my @s = sieve_prime_cluster(2**100, 2**100+1e12, 2,6,8,12,18,20);

Efficiently finds prime clusters between the first two arguments C<low>
and C<high> (inclusive).  The remaining arguments describe the cluster.
The cluster values must be even, less than 31 bits, and strictly increasing.
Given a cluster set C<C>, the returned values are all primes in the
range where C<p+c> is prime for all C<c> in the cluster set C<C>.

The cluster is described as offsets from 0, with the implicit prime
at 0.  Hence an empty list is asking for all primes (the cluster
C<p+0>).  A list with the single value C<2> will find all twin primes
(the cluster where C<p+0> and C<p+2> are prime).  The list C<2,6,8>
will find prime quadruplets.  Note that there is no requirement that
the list denote a constellation (a cluster with minimal distance) --
the list C<42,92,606> is just fine.

For long clusters, e.g. L<OEIS series A213601|http://oeis.org/A213601>
prime 12-tuplets, this will be immensely more efficient than filtering
out the cluster from a list of primes.  For that example, a range of
C<10^13> takes less than a second to search -- thousands of times faster
than filtering results from primes or twin primes.
Shorter clusters are not quite this efficient, and the overhead for
returning large arrays should not be ignored.


=head2 next_prime

  $n = next_prime($n);

Returns the prime following the input number (the smallest prime number
that is greater than the input number).
The function L</is_prob_prime> is used to determine when a prime is found,
hence the result is a probable prime (using BPSW).

For large inputs this function is quite a bit faster than GMP's
C<mpz_nextprime> or Pari's C<nextprime>.


=head2 prev_prime

  $n = prev_prime($n);

Returns the prime preceding the input number (the largest prime number
that is less than the input number).
0 is returned if the input is C<2> or lower.
The function L</is_prob_prime> is used to determine when a prime is found,
hence the result is a probable prime (using BPSW).


=head2 surround_primes

  ($dprev, $dnext) = surround_primes($n);

Returns the distances to the previous and next primes of the input C<n>.
This is slightly more efficient than calling both L</prev_prime> and
L</next_prime>, and returning the distances as native integers is more
efficient with large inputs.

If an optional second argument C<d> is given, and the input C<n> is larger
than C<2^64>, then if a SPSP-2 is found in the range C<n-d> to C<n+d>
(inclusive) then it will be returned with the other argument set to C<0>.
Otherwise, the first SPSP-2 values found are returned.
This feature is especially useful for prime gap searches as well as
finding the nearest prime to a value.

Note that with a non-zero second argument, the values returned have not
undergone a full BPSW test; just sieving and a SPSP-2 test.


=head2 random_nbit_prime

  say "random 512-bit prime: ", random_nbit_prime(512);

Returns a randomly selected prime of exactly C<n> bits.
C<undef> is returned if C<n> is less than C<2>.
The returned prime has passed the C<is_prob_prime> (extra strong BPSW) test.

=head2 random_strong_prime

  say "random 512-bit strong prime: ", random_strong_prime(512);

Returns a randomly selected strong prime of exactly C<n> bits.
C<n> must be at least C<256>.
The returned prime has passed the C<is_prob_prime> (extra strong BPSW) test.

Given the returned prime C<p>, C<p+1>, C<q=p-1>, and C<q-1> will all have
a large factor.  This makes using factoring methods such as p-1 and p+1 much
harder.  Gordon's algorithm is used.  The value of using strong primes is
questionable over proper random primes when the number of bits is
at least 1024.

=head2 random_ndigit_prime

  say "random 200-digit prime: ", random_ndigit_prime(200);

Returns a randomly selected prime of exactly C<n> digits.
C<undef> is returned if C<n> is less than C<1>.
The returned prime has passed the C<is_prob_prime> (extra strong BPSW) test.

=head2 random_prime

  say random_prime(1000, 2000);  # prime between 1000 and 2000 inclusive

Returns a random prime in the interval C<[a,b]> or C<undef> if no prime is
in the range.
The returned prime has passed the C<is_prob_prime> (extra strong BPSW) test.

The random prime functions use the internal CSPRNG for randomness.  This
is currently ISAAC-32 but will likely change to ChaCha20 in a later release.

This corresponds to Mathematica's C<RandomPrime[{min,max}]> function.
This is a superset of Pari's C<randomprime(n)> function, where our interval
API is more convenient for cryptographic functions.

=head2 random_maurer_prime

  say "random 512-bit proven prime: ", random_maurer_prime(512);

Returns an n-bit proven prime using Ueli Maurer's FastPrime algorithm (1995).
This results in uniform random selection of a proven prime, though not every
n-bit prime can be generated with this algorithm.

C<undef> is returned if C<n> is less than C<2>.
As a safety check, internally the extra strong BPSW test is additionally
run on each intermediate and the final result.

=head2 random_shawe_taylor_prime

  say "random 512-bit proven prime: ", random_shawe_taylor_prime(512);

Returns an n-bit proven prime using the Shawe-Taylor algorithm (1986) from
section C.6 of FIPS 186-4, although using our CSPRNG rather than SHA-256.
This is a slightly simpler and older (1986) method than Maurer's algorithm.
It is a bit faster than Maurer's method but has a smaller subset of returned
primes.

C<undef> is returned if C<n> is less than C<2>.
As a safety check, internally the extra strong BPSW test is additionally
run on each intermediate and the final result.

=head2 random_maurer_prime_with_cert

Like L</random_maurer_prime> but also returns a string certificate.

=head2 random_shawe_taylor_prime_with_cert

Like L</random_shawe_taylor_prime> but also returns a string certificate.


=head2 lucasu

  say "Fibonacci($_) = ", lucasu(1,-1,$_) for 0..100;

Given integers C<P>, C<Q>, and the non-negative integer C<k>,
computes C<U_k> for the Lucas sequence defined by C<P>,C<Q>.  These include
the Fibonacci numbers (C<1,-1>), the Pell numbers (C<2,-1>), the Jacobsthal
numbers (C<1,-2>), the Mersenne numbers (C<3,2>), and more.

This corresponds to OpenPFGW's C<lucasU> function and gmpy2's C<lucasu>
function.

=head2 lucasv

  say "Lucas($_) = ", lucasv(1,-1,$_) for 0..100;

Given integers C<P>, C<Q>, and the non-negative integer C<k>,
computes C<V_k> for the Lucas sequence defined by C<P>,C<Q>.  These include
the Lucas numbers (C<1,-1>).

This corresponds to OpenPFGW's C<lucasV> function and gmpy2's C<lucasv>
function.

=head2 lucas_sequence

  my($U, $V, $Qk) = lucas_sequence($n, $P, $Q, $k)

Computes C<U_k>, C<V_k>, and C<Q_k> for the Lucas sequence defined by
C<P>,C<Q>, modulo C<n>.  The modular Lucas sequence is used in a
number of primality tests and proofs.

The following conditions must hold:
  - C<< D = P*P - 4*Q  !=  0 >>
  - C<< P > 0 >>
  - C<< P < n >>
  - C<< Q < n >>
  - C<< k >= 0 >>
  - C<< n >= 2 >>


=head2 primorial

  $p = primorial($n);

Given an unsigned integer argument, returns the product of the prime numbers
which are less than or equal to C<n>.  This definition of C<n#> follows
L<OEIS series A034386|http://oeis.org/A034386> and
L<Wikipedia: Primorial definition for natural numbers|http://en.wikipedia.org/wiki/Primorial#Definition_for_natural_numbers>.

=head2 pn_primorial

  $p = pn_primorial($n)

Given an unsigned integer argument, returns the product of the first C<n>
prime numbers.  This definition of C<p_n#> follows
L<OEIS series A002110|http://oeis.org/A002110> and
L<Wikipedia: Primorial definition for prime numbers|http://en.wikipedia.org/wiki/Primorial#Definition_for_prime_numbers>.

The two are related with the relationships:

  pn_primorial($n)  ==   primorial( nth_prime($n) )
  primorial($n)     ==   pn_primorial( prime_count($n) )

=head2 factorial

Given positive integer argument C<n>, returns the factorial of C<n>,
defined as the product of the integers 1 to C<n> with the special case
of C<factorial(0) = 1>.  This corresponds to Pari's C<factorial(n)>
and Mathematica's C<Factorial[n]> functions.

=head2 multifactorial

Given two positive integer arguments C<n> and C<m>, returns C<n!^(m)>,
the multifactorial.  C<m=1> is the standard L</factorial> while C<m=2>
is the double factorial.  While the factorial is the product of all
integers C<n> and below, the multifactorial skips those without the
same parity as C<n mod m>.  Hence

  multifactorial(n,2) = n * (n-2) * (n-4) * ...

The multifactorials are the OEIS series
(m=1) L<OEIS series A000142|http://oeis.org/A000142>,
(m=2) L<OEIS series A006882|http://oeis.org/A006882>,
(m=3) L<OEIS series A007661|http://oeis.org/A007661>,
(m=4) L<OEIS series A007662|http://oeis.org/A007662>, ...

Also see L<Peter Luschny's excellent writeup|http://oeis.org/wiki/User:Peter_Luschny/Multifactorials>.

=head2 factorialmod

Given two positive integer arguments C<n> and C<m>, returns C<n! mod m>.
This is much faster than computing the large C<factorial(n)> followed
by a mod operation.

=head2 factorial_sum

Given positive integer argument C<n>, returns the factorial sum of C<n>.
This is defined as the sum of C<factorial(k)> for C<0 .. n-1>.  These
are equivalent, though this function is faster:

  factorial_sum($n) == vecsum(map{ factorial($_) } 0..$n-1)

This is sometimes called the left factorial, confusingly also used for
L</subfactorial>.  This is L<OEIS series A003422|http://oeis.org/A003422>.

=head2 subfactorial

Given positive integer argument C<n>, returns the subfactorial of C<n>.
This is also called the derangement number, and occasionally the left
factorial.

This is L<OEIS series A000166|http://oeis.org/A000166>.
This corresponds to Mathematica's C<Subfactorial[n]> function.

=head2 gcd

Given a list of integers, returns the greatest common divisor.  This is
often used to test for L<coprimality|https://oeis.org/wiki/Coprimality>.

=head2 lcm

Given a list of integers, returns the least common multiple.

=head2 gcdext

Given two integers C<x> and C<y>, returns C<u,v,d> such that C<d = gcd(x,y)>
and C<u*x + v*y = d>.  This uses the extended Euclidian algorithm to compute
the values satisfying Bézout's Identity.

This corresponds to Pari's C<gcdext> function, which was renamed from
C<bezout> in Pari 2.6.  The results will hence match L<Math::Pari/bezout>.

=head2 chinese

  say chinese( [14,643], [254,419], [87,733] );  # 87041638

Solves a system of simultaneous congruences using the Chinese Remainder
Theorem (with extension to non-coprime moduli).  A list of C<[a,n]> pairs
are taken as input, each representing an equation C<x ≡ a mod n>.  If no
solution exists, C<undef> is returned.  If a solution is returned, the
modulus is equal to the lcm of all the given moduli (see L</lcm>.  In
the standard case where all values of C<n> are coprime, this is just the
product.  The C<n> values must be positive integers, while the C<a> values
are integers.

=head2 vecsum

Returns the sum of all arguments, each of which must be an integer.

=head2 vecprod

Returns the product of all arguments, each of which must be an integer.

=head2 kronecker

Returns the Kronecker symbol C<(a|n)> for two integers.  The possible
return values with their meanings for odd positive C<n> are:

   0   a = 0 mod n
   1   a is a quadratic residue modulo n (a = x^2 mod n for some x)
  -1   a is a quadratic non-residue modulo n

The Kronecker symbol is an extension of the Jacobi symbol to all integer
values of C<n> from the latter's domain of positive odd values of C<n>.
The Jacobi symbol is itself an extension of the Legendre symbol, which is
only defined for odd prime values of C<n>.  This corresponds to Pari's
C<kronecker(a,n)> function and Mathematica's C<KroneckerSymbol[n,m]>
function.

=head2 binomial

Given integer arguments C<n> and C<k>, returns the binomial coefficient
C<n*(n-1)*...*(n-k+1)/k!>, also known as the choose function.  Negative
arguments use the L<Kronenburg extensions|http://arxiv.org/abs/1105.3689/>.
This corresponds to Mathematica's C<Binomial[n,k]> function, Pari's
C<binomial(n,k)> function, and GMP's C<mpz_bin_ui> function.

For negative arguments, this matches Mathematica.  Pari does not implement
the C<n E<lt> 0, k E<lt>= n> extension and instead returns C<0> for this
case.  GMP's API does not allow negative C<k> but otherwise matches.
L<Math::BigInt> does not implement any extensions and the results for
C<n E<lt> 0, k E<gt> 0> are undefined.

=head2 addreal
=head2 subreal
=head2 mulreal
=head2 divreal

Returns the corresponding basic math operation applied to the two inputs.
An optional third argument indicates the number of significant digits
(default 40) with the result rounded.

=head2 logreal

Returns the natural logarithm of the input C<n>.
An optional second argument indicates the number of significant digits
(default 40) with the result rounded.

For C<logreal(2)> we use Formula 25 from Gourdon and Sebah (2010).
For other values we use AGM (Sasaki and Kanada theta method).
Performance is 100-1000x faster than Math::BigFloat's GMP backend.
It is 10x slower than Pari/GP 2.10 and MPFR.

Negative inputs are returned as C<-log(-n)>, which matches L<bignum>.
Pari/GP and Mathematica return C<log(-n) + Pi*i>.

=head2 expreal

Returns C<e^n> for the input C<n>.
An optional second argument indicates the number of significant digits
(default 40) with the result rounded.

The implementation computes C<sinh(n)>, then C<e^x> from that.

=head2 powreal

Returns C<n^x> for the inputs C<n> and C<x>.
An optional third argument indicates the number of significant digits
(default 40) with the result rounded.

Like L<logreal> and L<expreal>, this is a basic math function that is
not available from the GMP library but implemented in MPFR.  Since the
latter is not always available, this can be useful to have.

=head2 rootreal

Returns the C<n>-th root of C<x> for the inputs C<n> and C<x>.
An optional third argument indicates the number of significant digits
(default 40) with the result rounded.

This is just C<powreal(x^(1/n)>, but allows C<n> to be easily specified
in full precision.

=head2 agmreal

Returns the Arithmetic-Geometric mean (AGM) of C<a> and C<b>.
An optional third argument indicates the number of significant digits
(default 40) with the result rounded.

Examples of use include elementary constants (e.g. C<Pi> and C<e>),
logs, exponentials, trigonometric functions, elliptic integrals,
computing pendulum periods, and more.

This corresponds to Pari's C<agm(x,y)> function, limited to positive
reals (Pari also handles negative, complex, p-adic, and power series
arguments).

=head2 bernfrac

Returns the Bernoulli number C<B_n> for an integer argument C<n>, as a
rational number.  Two values are returned, the numerator and denominator.
B_1 = 1/2.
This corresponds to Pari's C<bernfrac(n)> and Mathematica's C<BernoulliB>
functions.

=head2 bernreal

Returns the Bernoulli number C<B_n> for an integer argument C<n>, as a
string floating point.  An optional second argument indicates the number
of significant digits to be used, with the result rounded.  The default
is 40 digits.
This corresponds to Pari's C<bernreal> function and.

=head2 harmfrac

Returns the Harmonic number C<H_n> for an integer argument C<n>, as a
rational number.  Two values are returned, the numerator and denominator.
numbers are the sum of reciprocals of the first C<n> natural numbers:
C<1 + 1/2 + 1/3 + ... + 1/n>.
This corresponds to Mathematica's C<HarmonicNumber> function.

=head2 harmreal

Returns the Harmonic number C<H_n> for an integer argument C<n>, as a
string floating point.  An optional second argument indicates the number
of digits to be preserved past the decimal place, with a default of 40.

=head2 stirling

  say "s(14,2) = ", stirling(14, 2);
  say "S(14,2) = ", stirling(14, 2, 2);

Returns the Stirling numbers of either the first kind (default), the
second kind, or the third kind (the unsigned Lah numbers), with the kind
selected as an optional third argument.  It takes two non-negative integer
arguments C<n> and C<k> plus the optional C<type>.  This corresponds to Pari's
C<stirling(n,k,{type})> function and Mathematica's
C<StirlingS1> / C<StirlingS2> functions.

Stirling numbers of the first kind are C<-1^(n-k)> times the number of
permutations of C<n> symbols with exactly C<k> cycles.  Stirling numbers
of the second kind are the number of ways to partition a set of C<n>
elements into C<k> non-empty subsets.  The Lah numbers are the number of
ways to split a set of C<n> elements into C<k> non-empty lists.

=head2 zeta

Given a positive integer or float C<n>, returns the real Riemann Zeta
value as a string floating point.  An optional second argument indicates
the number of digits past the decimal point (default 40).

The implementation is algorithm 2 of Borwein (1991).  Performance with
integer inputs is good, but floating point arguments with high precision
will be slower than methods using MPFR.  L<Math::Prime::Util> will
try to use L<Math::MPFR> if possible.

=head2 li

Given a positive integer or float C<n>, returns the real Logarithmic Integral
as a string floating point.  An optional second argument indicates the number
of significant digits (default 40) with the result rounded.

The implementation uses Ramanjan's series.
This corresponds to Mathematica's C<Li> function.

=head2 ei

Given a positive integer or float C<n>, returns the real Exponential Integral
as a string floating point.  An optional second argument indicates the number
of significant digits (default 40) with the result rounded.

The implementation is simply li(exp(x)).

=head2 riemannr

Given a positive integer or float C<n>, returns the real Riemann R function
as a string floating point.  An optional second argument indicates the number
of significant digits (default 40) with the result rounded.

The implementation is the standard Gram series.
This corresponds to Mathematica's C<RiemannR> function.

=head2 lambertw

Given a float C<x>, returns the principal branch of the Lambert W function.
This solves for C<W> in the equation C<x = W*exp(W)>.  The input must not be
less than C<-1/e>.  This corresponds to Pari's C<lambertw>
function and Mathematica's C<ProductLog> / C<LambertW> function.


=head2 znorder

  $order = znorder(17, "100000000000000000000000065");

Given two positive integers C<a> and C<n>, returns the multiplicative order
of C<a> modulo C<n>.  This is the smallest positive integer C<k> such that
C<a^k ≡ 1 mod n>.  Returns 1 if C<a = 1>.  Returns undef if C<a = 0> or if
C<a> and C<n> are not coprime, since no value will result in 1 mod n.
This corresponds to Pari's C<znorder(Mod(a,n))> function and Mathematica's
C<MultiplicativeOrder[a,n]> function.


=head2 znprimroot

Given a positive integer C<n>, returns the smallest primitive root
of C<(Z/nZ)^*>, or C<undef> if no root exists.  A root exists when
C<euler_phi($n) == carmichael_lambda($n)>, which will be true for
all prime C<n> and some composites.

L<OEIS A033948|http://oeis.org/A033948> is a sequence of integers where
the primitive root exists, while L<OEIS A046145|http://oeis.org/A046145>
is a list of the smallest primitive roots, which is what this function
produces.


=head2 is_primitive_root

Given two non-negative numbers C<a> and C<n>, returns C<1> if C<a> is a
primitive root modulo C<n>, and C<0> if not.  If C<a> is a primitive root,
then C<euler_phi(n)> is the smallest C<e> for which C<a^e = 1 mod n>.

=head2 is_semiprime

Given a positive integer C<n>, returns 1 if C<n> is a semiprime, 0 otherwise.
A semiprime is the product of exactly two primes.

The boolean result is the same as C<scalar(factor(n)) == 2>, but this
function performs shortcuts that can greatly speed up the operation.

=head2 is_carmichael

Given a positive integer C<n>, returns 1 if C<n> is a Carmichael number,
0 otherwise.
These are composites that satisfy C<b^(n-1) ≡ 1 mod n> for all
C<1 E<lt> b E<lt> n> relatively prime to C<n>.
Alternately Korselt's theorem says these are composites such that C<n> is
square-free and C<p-1> divides C<n-1> for all prime divisors C<p> of C<n>.

Inputs greater than 50 digits use a probabilistic test to avoid fully
factoring the input.

=head2 is_fundamental

Given a positive integer C<n>, returns 1 if C<n> is a fundamental
discriminant, 0 otherwise.

=head2 is_totient

Given an integer C<n>, returns 1 if there exists an integer C<x> where
C<euler_phi(x) == n>.

=head2 is_polygonal

Given integers C<x> and C<s>, return 1 if x is an s-gonal number, 0 otherwise.
C<s> must be greater than 2.

=head2 polygonal_nth

Given integers C<x> and C<s>, return N if C<x> is the C<N-th> s-gonal number,
0 otherwise.


=head2 sigma

  say "Sum of divisors of $n:", sigma( $n );
  say "sigma_2($n) = ", sigma($n, 2);
  say "Number of divisors: sigma_0($n) = ", sigma($n, 0);

This function takes a positive integer as input and returns the sum of
its divisors, including 1 and itself.  An optional second argument C<k>
may be given, which will result in the sum of the C<k-th> powers of the
divisors to be returned.

This is known as the sigma function (see Hardy and Wright section 16.7,
or OEIS A000203).  The API is identical to Pari/GP's C<sigma> function.
This function is useful for calculating things like aliquot sums, abundant
numbers, perfect numbers, etc.


=head2 ramanujan_tau

Takes a positive integer as input and returns the value of Ramanujan's tau
function.  The result is a signed integer.
This corresponds to Mathematica's C<RamanujanTau> function
and Pari's C<ramanujantau> function.


=head2 valuation

  say "$n is divisible by 2 ", valuation($n,2), " times.";

Given integers C<n> and C<k>, returns the numbers of times C<n> is divisible
by C<k>.  This is a very limited version of the algebraic valuation meaning,
just applied to integers.
This corresponds to Pari's C<valuation> function.
C<0> is returned if C<n> or C<k> is one of the values C<-1>, C<0>, or C<1>.

=head2 hammingweight

Given an integer C<n>, returns the binary Hamming weight of C<abs(n)>.  This
is also called the population count, and is the number of 1s in the binary
representation.  This corresponds to Pari's C<hammingweight> function for
C<t_INT> arguments.

=head2 moebius

  say "$n is square free" if moebius($n) != 0;
  $sum += moebius($_) for (1..200); say "Mertens(200) = $sum";
  say "Mertens(2000) = ", vecsum(moebius(0,2000));

Returns μ(n), the Möbius function (also known as the Moebius, Mobius, or
MoebiusMu function) for an integer input.  This function is 1 if
C<n = 1>, 0 if C<n> is not square free (i.e. C<n> has a repeated factor),
and C<-1^t> if C<n> is a product of C<t> distinct primes.  This is an
important function in prime number theory.  Like SAGE, we define
C<moebius(0) = 0> for convenience.

If called with two arguments, they define a range C<low> to C<high>, and the
function returns an array with the value of the Möbius function for every n
from low to high inclusive.

=head2 invmod

  say "The inverse of 42 mod 2017 = ", invmod(42,2017);

Given two integers C<a> and C<n>, return the inverse of C<a> modulo C<n>.
If not defined, undef is returned.  If defined, then the return value
multiplied by C<a> equals C<1> modulo C<n>.

=head2 sqrtmod

Given two integers C<a> and C<p>, return the square root of C<a> mod C<p>.
If no square root exists, undef is returned.  If defined, the return value
C<s> will always satisfy C<mulmod(s,s,p) = a>.

If C<p> is not a prime, it is possible no result will be returned even
though a modular root exists.

Only one root is returned, even though there are at least two.  In the
case of C<p> a prime and a return value C<s>, then both C<+s mod n> and
C<-s mod n> are roots.  The least C<s> will be returned.  In the case of
composites, many roots may exist, but only one will be returned.

=head2 addmod

Given three integers C<a>, C<b>, and C<n> where C<a> and C<n> are unsigned,
return C<(a+b) mod n>.  This is particularly useful when dealing with
numbers that are larger than a half-word but still native size.  No
bigint package is needed and this can be 10-200x faster than using one.

=head2 mulmod

Given three integers C<a>, C<b>, and C<n> where C<a> and C<n> are unsigned,
return C<(a*b) mod n>.  This is particularly useful when C<n> fits in a
native integer.  No bigint package is needed and this can be 10-200x
faster than using one.

=head2 powmod

Given three integers C<a>, C<b>, and C<n> where C<a> and C<n> are unsigned,
return C<(a ** b) mod n>.  Typically binary exponentiation is used, so
the process is very efficient.  With native size inputs, no bigint
library is needed.

=head2 divmod

Given three integers C<a>, C<b>, and C<n> where C<a> and C<n> are unsigned,
return C<(a/b) mod n>.  This is done as C<(a * (1/b mod n)) mod n>.  If
no inverse of C<b> mod C<n> exists then undef if returned.


=head2 consecutive_integer_lcm

  $lcm = consecutive_integer_lcm($n);

Given an unsigned integer argument, returns the least common multiple of all
integers from 1 to C<n>.  This can be done by manipulation of the primes up
to C<n>, resulting in much faster and memory-friendly results than using
factorials.


=head2 partitions

Calculates the partition function p(n) for a non-negative integer input.
This is the number of ways of writing the integer n as a sum of positive
integers, without restrictions.  This corresponds to Pari's C<numbpart>
function and Mathematica's C<PartitionsP> function.  The values produced
in order are L<OEIS series A000041|http://oeis.org/A000041>.

This uses a combinatorial calculation, which means it will not be very
fast compared to Pari, Mathematica, or FLINT which use the Rademacher
formula using multi-precision floating point.  In 10 seconds, the pure
Perl version can produce C<partitions(10_000)> while with
L<Math::Prime::Util::GMP> it can do C<partitions(220_000)>.  In contrast,
in about 10 seconds Pari can solve C<numbpart(22_000_000)>.

If you want the enumerated partitions, see L<Math::Prime::Util/forpart>
or L<Integer::Partition>.  These are fast and memory efficient iterators,
but not practical for producing the partition I<number> for values
over 100 or so.


=head2 numtoperm

  @p = numtoperm(10,654321);  # @p=(1,8,2,7,6,5,3,4,9,0)

Given a non-negative integer C<n> and integer C<k>, return the
rank C<k> lexicographic permutation of C<n> elements.
C<k> will be interpreted as mod C<n!>.

=head2 permtonum

  $k = permtonum([1,8,2,7,6,5,3,4,9,0]);  # $k = 654321

Given an array reference containing integers from C<0> to C<n>,
returns the lexicographic permutation rank of the set.  This is
the inverse of the L</numtoperm> function.  All integers up to
C<n> must be present.  The result will be between C<0> and C<n!-1>.


=head2 Pi

Takes a positive integer argument C<n> and returns the constant Pi with that
many digits (including the leading 3).  Rounding is performed.

The implementation uses either AGM or Ramanujan/Chudnovsky with binary
splitting, depending on the number of digits.  It is a little over 2x
faster than MPFR, similar in speed to Pari/GP, but about 1.5x slower than
Xue's Chudnovsky demo from the GMP web site.
Specialized programs such as C<y-cruncher> are even faster.

Note there is a non-trivial amount of overhead in turning the result into
a string, as well as even more if using the L<ntheory> module which
further converts the result into a Math::BigFloat object.

Called in void context, this just calculates and caches the result.


=head2 Euler

Takes a positive integer argument C<n> and returns Euler's constant
with that many digits.  Rounding is performed.

The implementation is Brent-McMillan algorithm B, just like Pari/GP.
Performance is about 3x faster than Pari/GP, but 2-10x slower than MPFR
which uses binary splitting.

Called in void context, this just calculates and caches the result.


=head2 exp_mangoldt

  say "exp(lambda($_)) = ", exp_mangoldt($_) for 1 .. 100;

Returns EXP(Λ(n)), the exponential of the Mangoldt function (also known
as von Mangoldt's function) for an integer value.
The Mangoldt function is equal to log p if n is prime or a power of a prime,
and 0 otherwise.  We return the exponential so all results are integers.
Hence the return value for C<exp_mangoldt> is:

   p   if n = p^m for some prime p and integer m >= 1
   1   otherwise.


=head2 totient

  say "The Euler totient of $n is ", totient($n);

Returns φ(n), the Euler totient function (also called Euler's phi or phi
function) for an integer value.  This is an arithmetic function which counts
the number of positive integers less than or equal to C<n> that are relatively
prime to C<n>.  Given the definition used, C<totient> will return 0 for all
C<n E<lt> 1>.  This follows the logic used by SAGE.  Mathematica and Pari
return C<totient(-n)> for C<n E<lt> 0>.  Mathematica returns 0 for C<n = 0>,
Pari pre-2.6.2 raises and exception, and Pari 2.6.2 and newer returns 2.


=head2 jordan_totient

  say "Jordan's totient J_$k($n) is ", jordan_totient($k, $n);

Returns Jordan's totient function for a given integer value.  Jordan's totient
is a generalization of Euler's totient, where
  C<jordan_totient(1,$n) == euler_totient($n)>
This counts the number of k-tuples less than or equal to n that form a coprime
tuple with n.  As with C<totient>, 0 is returned for all C<n E<lt> 1>.
This function can be used to generate some other useful functions, such as
the Dedekind psi function, where C<psi(n) = J(2,n) / J(1,n)>.


=head2 carmichael_lambda

Returns the Carmichael function (also called the reduced totient function,
or Carmichael λ(n)) of a positive integer argument.  It is the smallest
positive integer C<m> such that C<a^m = 1 mod n> for every integer C<a>
coprime to C<n>.  This is L<OEIS series A002322|http://oeis.org/A002322>.


=head2 liouville

Returns λ(n), the Liouville function for a non-negative integer input.
This is -1 raised to Ω(n) (the total number of prime factors).


=head2 is_power

  say "$n is a perfect square" if is_power($n, 2);
  say "$n is a perfect cube" if is_power($n, 3);
  say "$n is a ", is_power($n), "-th power";

Given a single positive integer input C<n>, returns k if C<n = p^k> for
some integer C<p E<gt> 1, k E<gt> 1>, and 0 otherwise.  The k returned is
the largest possible.  This can be used in a boolean statement to
determine if C<n> is a perfect power.

If given two arguments C<n> and C<k>, returns 1 if C<n> is a C<k-th> power,
and 0 otherwise.  For example, if C<k=2> then this detects perfect squares.

This corresponds to Pari/GP's C<ispower> function, with the limitations of
only integer arguments and no third argument may be given to return the root.

=head2 is_square

Given a positive integer C<n>, returns 1 if C<n> is a perfect square,
0 otherwise.  This is identical to C<is_power(n,2)>.

This corresponds to Pari/GP's C<issquare> function.

=head2 is_prime_power

Given an integer input C<n>, returns C<k> if C<n = p^k> for some prime p,
and zero otherwise.

This corresponds to Pari/GP's C<isprimepower> function.

=head2 sqrtint

Returns the truncated integer part of the square root of C<n>.

This corresponds to Pari/GP's C<sqrtint> function.

=head2 rootint

Given C<n> and C<k>, returns the truncated integer part of the C<k-th> root
of C<n>.

This corresponds to Pari/GP's C<sqrtnint> function.

=head2 logint

Given C<n> and C<b>, returns the integer base-C<b> logarithm of C<n>.
This is the largest integer C<e> such that C<b^e E<lt>= n>.

This corresponds to Pari/GP's C<logint> function.

=head2 factor

  @factors = factor(640552686568398413516426919223357728279912327120302109778516984973296910867431808451611740398561987580967216226094312377767778241368426651540749005659);
  # Returns an array of 11 factors

Returns a list of prime factors of a positive number, in numerical order.  The
special cases of C<n = 0> and C<n = 1> will return C<n>.

Like most advanced factoring programs, a mix of methods is used.  This
includes trial division for small factors, perfect power detection,
Pollard's Rho, Pollard's P-1 with various smoothness and stage settings,
Hart's OLF (a Fermat variant), ECM (elliptic curve method), and
QS (quadratic sieve).
Certainly improvements could be designed for this algorithm
(suggestions are welcome).

In practice, this factors 26-digit semiprimes in under C<100ms>, 36-digit
semiprimes in under one second.  Arbitrary integers are factored faster.
It is many orders of magnitude faster than any other factoring module on
CPAN circa 2013.  It is comparable in speed to Math::Pari's C<factorint>
for most inputs.

If you want better factoring in general, I recommend looking at the
standalone programs
L<yafu|http://sourceforge.net/projects/yafu/>,
L<msieve|http://sourceforge.net/projects/msieve/>,
L<gmp-ecm|http://ecm.gforge.inria.fr/>, and
L<GGNFS|http://sourceforge.net/projects/ggnfs/>.


=head2 divisors

  my @divisors = divisors(30);   # returns (1, 2, 3, 5, 6, 10, 15, 30)

Produces all the divisors of a positive number input, including 1 and
the input number.  The divisors are a power set of multiplications of
the prime factors, returned as a sorted list with no duplications.
The result is identical to that of Pari's C<divisors> and
Mathematica's C<Divisors[n]> functions.

In scalar context this returns the sigma0 function (OEIS A000005), and
is the same result as evaluating the returned array in scalar context
(but much more efficient).
The result then corresponds to Pari's C<numdiv> and Mathematica's
C<DivisorSigma[0,n]> functions.


=head2 trial_factor

  my @factors = trial_factor($n);
  my @factors = trial_factor($n, 1000);

Given a positive number input, tries to discover a factor using trial division.
The resulting array will contain either two factors (it succeeded) or the
original number (no factor was found).  In either case, multiplying @factors
yields the original input.  An optional divisor limit may be given as the
second parameter.  Factoring will stop when the input is a prime, one factor
is found, or the input has been tested for divisibility with all primes less
than or equal to the limit.  If no limit is given, then C<2**31-1> will be used.

This is a good and fast initial test, and will be very fast for small numbers
(e.g. under 1 million).  For larger numbers, faster methods for complete
factoring have been known since the 17th century.

For inputs larger than about 1000 digits, a dynamic product/remainder tree
is used, which is faster than GMP's native methods.  This helps when pruning
composites or looking for very small factors.


=head2 prho_factor

  my @factors = prho_factor($n);
  my @factors = prho_factor($n, 100_000_000);

Given a positive number input, tries to discover a factor using Pollard's Rho
method.  The resulting array will contain either two factors (it succeeded)
or the original number (no factor was found).  In either case, multiplying
@factors yields the original input.  An optional number of rounds may be
given as the second parameter.  Factoring will stop when the input is a prime,
one factor has been found, or the number of rounds has been exceeded.

This is the Pollard Rho method with C<f = x^2 + 3> and default rounds 64M.  It
is very good at finding small factors.  Typically L</pbrent_factor> will be
preferred as it behaves similarly but runs quite a bit faster.  They use
different parameters however, so are not completely identical.


=head2 pbrent_factor

  my @factors = pbrent_factor($n);
  my @factors = pbrent_factor($n, 100_000_000);

Given a positive number input, tries to discover a factor using Pollard's Rho
method with Brent's algorithm.  The resulting array will contain either two
factors (it succeeded) or the original number (no factor was found).  In
either case, multiplying @factors yields the original input.  An optional
number of rounds may be given as the second parameter.  Factoring will stop
when the input is a prime, one factor has been found, or the number of
rounds has been exceeded.

This is the Pollard Rho method using Brent's modified cycle detection,
delayed C<gcd> computations, and backtracking.  It is essentially
Algorithm P''2 from Brent (1980).  Parameters used are C<f = x^2 + 3>
and default rounds 64M.  It is very good at finding small factors.


=head2 pminus1_factor

  my @factors = pminus1_factor($n);

  # Set B1 smoothness to 10M, second stage automatically set.
  my @factors = pminus1_factor($n, 10_000_000);

  # Run p-1 with B1 = 10M, B2 = 100M.
  my @factors = pminus1_factor($n, 10_000_000, 100_000_000);

Given a positive number input, tries to discover a factor using Pollard's
C<p-1> method.  The resulting array will contain either two factors (it
succeeded) or the original number (no factor was found).  In either case,
multiplying @factors yields the original input.  An optional first stage
smoothness factor (B1) may be given as the second parameter.  This will be
the smoothness limit B1 for the first stage, and will use C<10*B1> for
the second stage limit B2.  If a third parameter is given, it will be used
as the second stage limit B2.
Factoring will stop when the input is a prime, one factor has been found, or
the algorithm fails to find a factor with the given smoothness.

This is Pollard's C<p-1> method using a default smoothness of 5M and a
second stage of C<B2 = 10 * B1>.  It can quickly find a factor C<p> of the
input C<n> if the number C<p-1> factors into small primes.  For example
C<n = 22095311209999409685885162322219> has the factor C<p = 3916587618943361>,
where C<p-1 = 2^7 * 5 * 47 * 59 * 3137 * 703499>, so this method will find
a factor in the first stage if C<B1 E<gt>= 703499> or in the second stage if
C<B1 E<gt>= 3137> and C<B2 E<gt>= 703499>.

The implementation is written from scratch using the basic algorithm including
a second stage as described in Montgomery 1987.  It is faster than most simple
implementations I have seen (many of which are written assuming native
precision inputs), but slower than Ben Buhrow's code used in earlier
versions of L<yafu|http://sourceforge.net/projects/yafu/>, and nowhere close
to the speed of the version included with modern GMP-ECM with large B values
(it is actually quite a bit faster than GMP-ECM with small smoothness values).


=head2 pplus1_factor

  my @factors = pplus1_factor($n);

Given a positive number input, tries to discover a factor using Williams'
C<p+1> method.  The resulting array will contain either two factors (it
succeeded) or the original number (no factor was found).  In either case,
multiplying @factors yields the original input.  An optional first stage
smoothness factor (B1) may be given as the second parameter.  This will be
the smoothness limit B1 for the first stage.
Factoring will stop when the input is a prime, one factor has been found,
or the algorithm fails to find a factor with the given smoothness.



=head2 holf_factor

  my @factors = holf_factor($n);
  my @factors = holf_factor($n, 100_000_000);

Given a positive number input, tries to discover a factor using Hart's OLF
method.  The resulting array will contain either two factors (it succeeded)
or the original number (no factor was found).  In either case, multiplying
@factors yields the original input.  An optional number of rounds may be
given as the second parameter.  Factoring will stop when the input is a
prime, one factor has been found, or the number of rounds has been exceeded.

This is Hart's One Line Factorization method, which is a variant of Fermat's
algorithm.  A premultiplier of 480 is used.  It is very good at factoring
numbers that are close to perfect squares, or small numbers.  Very naive
methods of picking RSA parameters sometimes yield numbers in this form, so
it can be useful to run a few rounds to check.  For example, the number:

  18548676741817250104151622545580576823736636896432849057 \
  10984160646722888555430591384041316374473729421512365598 \
  29709849969346650897776687202384767704706338162219624578 \
  777915220190863619885201763980069247978050169295918863

was proposed by someone as an RSA key.  It is indeed composed of two distinct
prime numbers of similar bit length.  Most factoring methods will take a
B<very> long time to break this.  However one factor is almost exactly 5x
larger than the other, allowing HOLF to factor this 222-digit semiprime in
only a few milliseconds.


=head2 squfof_factor

  my @factors = squfof_factor($n);
  my @factors = squfof_factor($n, 100_000_000);

Given a positive number input, tries to discover a factor using Shanks'
square forms factorization method (usually known as SQUFOF).  The resulting
array will contain either two factors (it succeeded) or the original number
(no factor was found).  In either case, multiplying @factors yields the
original input.  An optional number of rounds may be given as the second
parameter.  Factoring will stop when the input is a prime, one factor has
been found, or the number of rounds has been exceeded.

This is Daniel Shanks' SQUFOF (square forms factorization) algorithm.  The
particular implementation is a non-racing multiple-multiplier version, based
on code ideas of Ben Buhrow and Jason Papadopoulos as well as many others.
SQUFOF is often the preferred method for small numbers, and L<Math::Prime::Util>
as well as many other packages use it was the default method for native size
(e.g. 32-bit or 64-bit) numbers after trial division.  The GMP version used
in this module will work for larger values, but my testing indicates it is
generally slower than the C<prho> and C<pbrent> implementations.


=head2 ecm_factor

  my @factors = ecm_factor($n);
  my @factors = ecm_factor($n, 12500);      # B1 = 12500
  my @factors = ecm_factor($n, 12500, 10);  # B1 = 12500, curves = 10

Given a positive number input, tries to discover a factor using ECM.  The
resulting array will contain either two factors (it succeeded) or the original
number (no factor was found).  In either case, multiplying @factors yields the
original input.  An optional maximum smoothness may be given as the second
parameter, which relates to the size of factor to search for.  An optional
third parameter indicates the number of random curves to use at each
smoothness value being searched.

This is an implementation of Hendrik Lenstra's elliptic curve factoring
method, usually referred to as ECM.  The implementation is reasonable,
using projective coordinates, Montgomery's PRAC heuristic for EC
multiplication, and two stages.
It is much slower than the latest GMP-ECM, but still quite useful for
factoring reasonably sized inputs.


=head2 qs_factor

  my @factors = qs_factor($n);

Given a positive number input, tries to discover factors using QS (the
quadratic sieve).  The resulting array will contain one or more numbers such
that multiplying @factors yields the original input.  Typically multiple
factors will be produced, unlike the other C<..._factor> routines.

The current implementation is a modified version of SIMPQS, a predecessor to
the QS in FLINT, and was written by William Hart in 2006.  It will not operate
on input less than 30 digits.  The memory use for large inputs is more than
desired, so other methods such as L</pbrent_factor>, L</pminus1_factor>, and
L</ecm_factor> are recommended to begin with to filter out small factors.
However, it is substantially faster than the other methods on large inputs
having large factors, and is the method of choice for 35+ digit semiprimes.


=head2 todigits

Given an integer C<n>, return an array of digits of C<|n|>.  An optional
second integer argument specifies a base (default 10).  For example,
given a base of 2, this returns an array of binary digits of C<n>.
An optional third argument specifies a length for the returned array.
The result will be either have upper digits truncated or have leading
zeros added.

C<todigits(0)> returns an empty array.
The base must be at least 2, and is limited to an int.
Length must be at least zero and is limited to an int.


=head2 seed_csprng

Takes a non-negative integer C<nbytes> and a string C<data> as input.
These are used to seed the internal CSPRNG used for random functions,
including the random prime functions.  Ideally this is 16-256 bytes of
good entropy.

Currently the CSPRNG is ISAAC-32, and the maximum number of seed bytes
used is 1024.  The CSPRNG will likely change to ChaCha20 in a later release.


=head2 is_csprng_well_seeded

Returns true if the CSPRNG has been seeded with 16 or more bytes (128 bits).
There is no measurement of how "good" the input was.

On startup the module will attempt to seed the CSPRNG from
C</dev/urandom>, so this function will return true if that was
successful, but false otherwise.

=head2 urandomb

  $n32 = urandomb(32);    # Classic irand32, returns a UV
  $n   = urandomb(1024);  # Random integer less than 2^1024

Given a number of bits C<b>, returns a random unsigned integer less than C<2^b>.
The result will be uniformly distributed between C<0> and C<2^b-1> inclusive.

This is similar to the GMP function C<mpz_urandomb>.

=head2 urandomm

  $n = urandomm(100);    # random integer in [0,99]
  $n = urandomm(1024);   # random integer in [0,1023]

Given a positive integer C<n>, returns a random unsigned integer less than C<n>.
The results will be uniformly distributed between C<0> and C<n-1> inclusive.

This is similar to the GMP function C<mpz_urandomm>.

=head2 urandomr

  $n  = urandomr(100, 110);        # Random number [100,110]
  $nb = urandomr(2**24,2**25-1);   # Random 25-bit number
  $nd = urandomr(10**24,10**25-1); # Random 25-digit number

Given values C<low> and C<high>, returns a uniform random unsigned integer
in the range C<[low,high]>.  Both inputs must be non-negative.
If C<low E<gt> high> then function will return C<undef>.
Note that the range is inclusive, so C<low>, C<high>, and each integer
between them have an equal probability of appearing.

=head2 irand

  $n32 = irand;     # random 32-bit integer

Returns a random 32-bit integer using the CSPRNG.

Performance is similar to
L<Math::Random::MTwist/rand> and L<Math::Random::Xorshift>.
It is somewhat faster than casting system C<rand> to a 32-bit int.
It is noticeably faster than
L<Math::Random::ISAAC>,
L<Math::Random::ISAAC::XS>,
L<Math::Random::MT>,
L<Math::Random::MT::Auto>,
and L<Crypt::PRNG>.

=head2 irand64

  $n64 = irand64;   # random 64-bit integer

Returns a random 64-bit integer using the CSPRNG (on 64-bit Perl).

=head2 drand

  $f = drand;       # random floating point value in [0,1)
  $r = drand(25);   # random floating point value in [0,25)

Returns a random NV (Perl's native floating point) using the CSPRNG.

The number of bits returned is equal to the mantissa bits of the NV type
used for the Perl build, with a max of 64.  By default Perl uses doubles
and the returned values have 53 bits.  If Perl is built with long double
support and the long doubles have a larger mantissa, then more bits are
used.

This gives I<substantially> better quality random numbers than the default
Perl C<rand> function.  Among other things, on modern Perl's, C<rand> uses
drand48, which gives 32 bits of decent random values and 16 more bits of
known patterns (e.g. the 48th bit alternates, the 47th has a period
of 4, etc.).  There are much better choices for standard random number
generators, such as the Mersenne Twister from L<Math::Random::MTwist>.

Performance is similar to
L<Math::Random::MTwist/rand> and L<Math::Random::Xorshift>.
It is 1.5 - 2x slower than core C<rand> (as are the other modules).

=head2 random_bytes

  $str = random_bytes(32);     # 32 random bytes

Given an unsigned number C<n> of bytes, returns a binary string filled with
random data from the CSPRNG.  Performance for getting 256 byte strings:

    Module/Method                  Rate   Type
    -------------             ---------   ----------------------
    Data::Entropy::Algorithms    2027/s   CSPRNG - AES Counter
    Crypt::Random                6649/s   CSPRNG - /dev/urandom
    Bytes::Random                9217/s   drand48
    Bytes::Random::Secure       23043/s   CSPRNG - ISAAC
    Math::Random::ISAAC::XS     58377/s   CSPRNG - ISAAC
    rand+pack                   82977/s   drand48
    Crypt::PRNG                298567/s   CSPRNG - Fortuna
    Bytes::Random::XS          383354/s   drand48
    ntheory                    770364/s   CSPRNG - ChaCha20
    Math::Random::MTwist      1890151/s   Mersenne Twister
    Math::Prime::Util::GMP    2045715/s   CSPRNG - ISAAC

Each of the CSPRNG modules should be high quality.  There are no known
flaws in any of ISAAC, AES CTR, ChaCha20, or Fortuna.  The industry
seems to be standardizing on ChaCha20 (e.g. BSD, Linux, TLS).


=head1 SEE ALSO

=over 4

=item L<Math::Prime::Util>
Has many more functions, lots of fast code for dealing with native-precision
arguments (including much faster primes using sieves), and will use this
module when needed for big numbers.  Using L<Math::Prime::Util> rather than
this module directly is recommended.

=item L<Math::Primality> (version 0.08)
A Perl module with support for the strong Miller-Rabin test, strong
Lucas-Selfridge test, the BPSW probable prime test, next_prime / prev_prime,
the AKS primality test, and prime_count.  It uses L<Math::GMPz> to do all
the calculations, so is faster than pure Perl bignums, but a lot slower
than XS+GMP.  The prime_count function is only usable for very small inputs,
but the other functions are quite good for big numbers.  Make sure to use
version 0.05 or newer.

=item L<Math::Pari>
Supports quite a bit of the same functionality (and much more).  See
L<Math::Prime::Util/"SEE ALSO"> for more detailed information on how the
modules compare.

=item L<yafu|http://sourceforge.net/projects/yafu/>,
L<msieve|http://sourceforge.net/projects/msieve/>,
L<gmp-ecm|http://ecm.gforge.inria.fr/>,
L<GGNFS|http://sourceforge.net/projects/ggnfs/>
Good general purpose factoring utilities.  These will be faster than this
module, and B<much> better as the factor increases in size.

=item L<Primo|http://www.ellipsa.eu/public/primo/primo.html>
is the state of the art in freely available (though not open source!)
primality proving programs.  If you have 1000+ digit numbers to prove,
you want to use this.

=item L<mpz_aprcl|http://sourceforge.net/projects/mpzaprcl/>
Open source APR-CL primality proof implementation.
Fast primality proving, though without certificates.

=back


=head1 REFERENCES

=over 4

=item Robert Baillie and Samuel S. Wagstaff, Jr., "Lucas Pseudoprimes", Mathematics of Computation, v35 n152, October 1980, pp 1391-1417.  L<http://mpqs.free.fr/LucasPseudoprimes.pdf>

=item Daniel J. Bernstein, "Proving Primality After Agrawal-Kayal-Saxena",
preprint, Jan 2003.  L<http://cr.yp.to/papers/aks.pdf>

=item Jon Grantham, "Frobenius Pseudoprimes", Mathematics of Computation, v70 n234, March 2000, pp 873-891.  L<http://www.ams.org/journals/mcom/2001-70-234/S0025-5718-00-01197-2/>

=item John Brillhart, D. H. Lehmer, and J. L. Selfridge, "New Primality Criteria and Factorizations of 2^m +/- 1", Mathematics of Computation, v29, n130, Apr 1975, pp 620-647.  L<http://www.ams.org/journals/mcom/1975-29-130/S0025-5718-1975-0384673-1/S0025-5718-1975-0384673-1.pdf>

=item Richard P. Brent, "An improved Monte Carlo factorization algorithm", BIT 20, 1980, pp. 176-184.  L<http://www.cs.ox.ac.uk/people/richard.brent/pd/rpb051i.pdf>

=item Peter L. Montgomery, "Speeding the Pollard and Elliptic Curve Methods of Factorization", Mathematics of Computation, v48, n177, Jan 1987, pp 243-264.  L<http://www.ams.org/journals/mcom/1987-48-177/S0025-5718-1987-0866113-7/>

=item Richard P. Brent, "Parallel Algorithms for Integer Factorisation", in Number Theory and Cryptography, Cambridge University Press, 1990, pp 26-37.  L<http://www.cs.ox.ac.uk/people/richard.brent/pd/rpb115.pdf>

=item Richard P. Brent, "Some Parallel Algorithms for Integer Factorisation", in Proc. Third Australian Supercomputer Conference, 1999. (Note: there are multiple versions of this paper)  L<http://www.cs.ox.ac.uk/people/richard.brent/pd/rpb193.pdf>

=item William B. Hart, "A One Line Factoring Algorithm", preprint.  L<http://wstein.org/home/wstein/www/home/wbhart/onelinefactor.pdf>

=item Daniel Shanks, "SQUFOF notes", unpublished notes, transcribed by Stephen McMath.  L<http://www.usna.edu/Users/math/wdj/mcmath/shanks_squfof.pdf>

=item Jason E. Gower and Samuel S. Wagstaff, Jr, "Square Form Factorization", Mathematics of Computation, v77, 2008, pages 551-588.  L<http://homes.cerias.purdue.edu/~ssw/squfof.pdf>

=item A.O.L. Atkin and F. Morain, "Elliptic Curves and primality proving", Mathematics of Computation, v61, 1993, pages 29-68.  L<http://www.ams.org/journals/mcom/1993-61-203/S0025-5718-1993-1199989-X/>

=item R.G.E. Pinch, "Some Primality Testing Algorithms", June 1993.  Describes the primality testing methods used by many CAS systems and how most were compromised.  Gives recommendations for primality testing APIs.  L<http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.33.4409>

=back


=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>

Jason Papadopoulos wrote the tinyqs code which is basically unchanged.
William Hart wrote SIMPQS which is the basis for the QS code.


=head1 ACKNOWLEDGEMENTS

Obviously none of this would be possible without the mathematicians who
created and published their work.  Eratosthenes, Gauss, Euler, Riemann,
Fermat, Lucas, Baillie, Pollard, Brent, Montgomery, Shanks, Hart, Wagstaff,
Dixon, Pomerance, A.K. Lenstra, H. W. Lenstra Jr., Atkin, Knuth, etc.

The GNU GMP team, whose product allows me to concentrate on coding high-level
algorithms and not worry about any of the details of how modular exponentiation
and the like happen, and still get decent performance for my purposes.

Ben Buhrow and Jason Papadopoulos deserve special mention for their open
source factoring tools, which are both readable and fast.  In particular I am
leveraging their work on SQUFOF in the current implementation.
They are a huge resource to the community.

Jonathan Leto and Bob Kuo, who wrote and distributed the L<Math::Primality>
module on CPAN.  Their implementation of BPSW provided the motivation I needed
to do it in this module and L<Math::Prime::Util>.  I also used their
module quite a bit for testing against.

Paul Zimmermann's papers and GMP-ECM code were of great value for my projective
ECM implementation, as well as the many papers by Brent and Montgomery.


=head1 COPYRIGHT

Copyright 2011-2017 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

SIMPQS Copyright 2006, William Hart.  SIMPQS is distributed under GPL v2+.

=cut
