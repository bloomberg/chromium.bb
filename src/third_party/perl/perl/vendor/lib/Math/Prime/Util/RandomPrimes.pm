package Math::Prime::Util::RandomPrimes;
use strict;
use warnings;
use Carp qw/carp croak confess/;
use Math::Prime::Util qw/ prime_get_config
                          verify_prime
                          is_provable_prime_with_cert
                          primorial prime_count nth_prime
                          is_prob_prime is_strong_pseudoprime
                          next_prime prev_prime
                          urandomb urandomm random_bytes
                        /;

BEGIN {
  $Math::Prime::Util::RandomPrimes::AUTHORITY = 'cpan:DANAJ';
  $Math::Prime::Util::RandomPrimes::VERSION = '0.73';
}

BEGIN {
  do { require Math::BigInt;  Math::BigInt->import(try=>"GMP,Pari"); }
    unless defined $Math::BigInt::VERSION;

  use constant OLD_PERL_VERSION=> $] < 5.008;
  use constant MPU_MAXBITS     => (~0 == 4294967295) ? 32 : 64;
  use constant MPU_64BIT       => MPU_MAXBITS == 64;
  use constant MPU_32BIT       => MPU_MAXBITS == 32;
  use constant MPU_MAXPARAM    => MPU_32BIT ? 4294967295 : 18446744073709551615;
  use constant MPU_MAXDIGITS   => MPU_32BIT ?         10 : 20;
  use constant MPU_USE_XS      => prime_get_config->{'xs'};
  use constant MPU_USE_GMP     => prime_get_config->{'gmp'};

  *_bigint_to_int = \&Math::Prime::Util::_bigint_to_int;
}

################################################################################

# These are much faster than straightforward trial division when n is big.
# You'll want to first do a test up to and including 23.
my @_big_gcd;
my $_big_gcd_top = 20046;
my $_big_gcd_use = -1;
sub _make_big_gcds {
  return if $_big_gcd_use >= 0;
  if (prime_get_config->{'gmp'}) {
    $_big_gcd_use = 0;
    return;
  }
  if (Math::BigInt->config()->{lib} !~ /^Math::BigInt::(GMP|Pari)/) {
    $_big_gcd_use = 0;
    return;
  }
  $_big_gcd_use = 1;
  my $p0 = primorial(Math::BigInt->new( 520));
  my $p1 = primorial(Math::BigInt->new(2052));
  my $p2 = primorial(Math::BigInt->new(6028));
  my $p3 = primorial(Math::BigInt->new($_big_gcd_top));
  $_big_gcd[0] = $p0->bdiv(223092870)->bfloor->as_int;
  $_big_gcd[1] = $p1->bdiv($p0)->bfloor->as_int;
  $_big_gcd[2] = $p2->bdiv($p1)->bfloor->as_int;
  $_big_gcd[3] = $p3->bdiv($p2)->bfloor->as_int;
}

################################################################################


################################################################################



# For random primes, there are two good papers that should be examined:
#
#  "Fast Generation of Prime Numbers and Secure Public-Key
#   Cryptographic Parameters" by Ueli M. Maurer, 1995
#  http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.26.2151
#  related discussions:
#      http://www.daimi.au.dk/~ivan/provableprimesproject.pdf
#      Handbook of Applied Cryptography by Menezes, et al.
#
#  "Close to Uniform Prime Number Generation With Fewer Random Bits"
#   by Pierre-Alain Fouque and Mehdi Tibouchi, 2011
#   http://eprint.iacr.org/2011/481
#
#  Some things to note:
#
#    1) Joye and Paillier have patents on their methods.  Never use them.
#
#    2) The easy method of next_prime(random number), known as PRIMEINC, is
#       fast but gives a terrible distribution.  It has a positive bias and
#       most importantly the probability for a prime is proportional to its
#       gap, meaning some numbers in the range will be thousands of times
#       more likely than others).  On the contrary however, nobody has a way
#       to exploit this, and it's not-uncommon to see used.
#
# We use:
#   TRIVIAL range within native integer size (2^32 or 2^64)
#   FTA1    random_nbit_prime with 65+ bits
#   INVA1   other ranges with 65+ bit range
# where
#   TRIVIAL = monte-carlo method or equivalent, perfect uniformity.
#   FTA1    = Fouque/Tibouchi A1, very close to uniform
#   INVA1   = inverted FTA1, less uniform but works with arbitrary ranges
#
# The random_maurer_prime function uses Maurer's FastPrime algorithm.
#
# If Math::Prime::Util::GMP is installed, these functions will be many times
# faster than other methods (e.g. Math::Pari monte-carlo or Crypt::Primes).
#
# Timings on Macbook.
# The "with GMP" numbers use Math::Prime::Util::GMP 0.44.
# The "no GMP" numbers are with no Math::BigInt backend, so very slow in comparison.
# If another backend was used (GMP, Pari, LTM) it would be more comparable.
#
#                   random_nbit_prime         random_maurer_prime
#    n-bits       no GMP   w/ MPU::GMP        no GMP   w/ MPU::GMP
#    ----------  --------  -----------       --------  -----------
#       24-bit        1uS      same             same       same
#       64-bit        5uS      same             same       same
#      128-bit     0.12s          70uS         0.29s         166uS
#      256-bit     0.66s         379uS         1.82s         800uS
#      512-bit     7.8s        0.0022s        16.2s        0.0044s
#     1024-bit    ----         0.019s        ----          0.037s
#     2048-bit    ----         0.23s         ----          0.35s
#     4096-bit    ----         2.4s          ----          5.2s
#
# Random timings for 10M calls on i4770K:
#    0.39   Math::Random::MTwist 0.13
#    0.41   ntheory                                      <==== us
#    0.89   system rand
#    1.76   Math::Random::MT::Auto
#    5.35   Bytes::Random::Secure OO       w/ISAAC::XS
#    7.43   Math::Random::Secure           w/ISAAC::XS
#   12.40   Math::Random::Secure
#   12.78   Bytes::Random::Secure OO
#   13.86   Bytes::Random::Secure function w/ISAAC::XS
#   21.95   Bytes::Random::Secure function
#  822.1    Crypt::Random
#
# time perl -E 'use Math::Random::MTwist "irand32"; irand32() for 1..10000000;'
# time perl -E 'sub irand {int(rand(4294967296));} irand() for 1..10000000;'
# time perl -E 'use Math::Random::MT::Auto; sub irand { Math::Random::MT::Auto::irand() & 0xFFFFFFFF } irand() for 1..10000000;'
# time perl -E 'use Math::Random::Secure qw/irand/; irand() for 1..10000000;'
# time perl -E 'use Bytes::Random::Secure qw/random_bytes/; sub irand {return unpack("L",random_bytes(4));} irand() for 1..10000000;'
# time perl -E 'use Bytes::Random::Secure; my $rng = Bytes::Random::Secure->new(); sub irand {return $rng->irand;} irand() for 1..10000000;'
# time perl -E 'use Crypt::Random qw/makerandom/; sub irand {makerandom(Size=>32, Uniform=>1, Strength=>0)} irand() for 1..100_000;'
# > haveged daemon running to stop /dev/random blocking
# > Both BRS and CR have more features that this isn't measuring.
#
# To verify distribution:
#   perl -Iblib/lib -Iblib/arch -MMath::Prime::Util=:all -E 'my %freq; $n=1000000; $freq{random_nbit_prime(6)}++ for (1..$n); printf("%4d %6.3f%%\n", $_, 100.0*$freq{$_}/$n) for sort {$a<=>$b} keys %freq;'
#   perl -Iblib/lib -Iblib/arch -MMath::Prime::Util=:all -E 'my %freq; $n=1000000; $freq{random_prime(1260437,1260733)}++ for (1..$n); printf("%4d %6.3f%%\n", $_, 100.0*$freq{$_}/$n) for sort {$a<=>$b} keys %freq;'

# Sub to call with low and high already primes and verified range.
my $_random_prime = sub {
    my($low,$high) = @_;
    my $prime;

    #{ my $bsize = 100; my @bins; my $counts = 10000000;
    #  for my $c (1..$counts) { $bins[ $_IRANDF->($bsize-1) ]++; }
    #  for my $b (0..$bsize) {printf("%4d %8.5f%%\n", $b, $bins[$b]/$counts);} }

    # low and high are both odds, and low < high.

    # This is fast for small values, low memory, perfectly uniform, and
    # consumes the minimum amount of randomness needed.  But it isn't feasible
    # with large values.  Also note that low must be a prime.
    if ($high <= 262144 && MPU_USE_XS) {
      my $li     = prime_count(2, $low);
      my $irange = prime_count($low, $high);
      my $rand = urandomm($irange);
      return nth_prime($li + $rand);
    }

    $low-- if $low == 2;  # Low of 2 becomes 1 for our program.
    # Math::BigInt::GMP's RT 71548 will wreak havoc if we don't do this.
    $low = Math::BigInt->new("$low") if ref($high) eq 'Math::BigInt';
    confess "Invalid _random_prime parameters: $low, $high" if ($low % 2) == 0 || ($high % 2) == 0;

    # We're going to look at the odd numbers only.
    my $oddrange = (($high - $low) >> 1) + 1;

    croak "Large random primes not supported on old Perl"
      if OLD_PERL_VERSION && MPU_64BIT && $oddrange > 4294967295;

    # If $low is large (e.g. >10 digits) and $range is small (say ~10k), it
    # would be fastest to call primes in the range and randomly pick one.  I'm
    # not implementing it now because it seems like a rare case.

    # If the range is reasonably small, generate using simple Monte Carlo
    # method (aka the 'trivial' method).  Completely uniform.
    if ($oddrange < MPU_MAXPARAM) {
      my $loop_limit = 2000 * 1000;  # To protect against broken rand
      if ($low > 11) {
        while ($loop_limit-- > 0) {
          $prime = $low + 2 * urandomm($oddrange);
          next if !($prime % 3) || !($prime % 5) || !($prime % 7) || !($prime % 11);
          return $prime if is_prob_prime($prime);
        }
      } else {
        while ($loop_limit-- > 0) {
          $prime = $low + 2 * urandomm($oddrange);
          next if $prime > 11 && (!($prime % 3) || !($prime % 5) || !($prime % 7) || !($prime % 11));
          return 2 if $prime == 1;  # Remember the special case for 2.
          return $prime if is_prob_prime($prime);
        }
      }
      croak "Random function broken?";
    }

    # We have an ocean of range, and a teaspoon to hold randomness.

    # Since we have an arbitrary range and not a power of two, I don't see how
    # Fouque's algorithm A1 could be used (where we generate lower bits and
    # generate random sets of upper).  Similarly trying to simply generate
    # upper bits is full of ways to trip up and get non-uniform results.
    #
    # What I'm doing here is:
    #
    #   1) divide the range into semi-evenly sized partitions, where each part
    #      is as close to $rand_max_val as we can.
    #   2) randomly select one of the partitions.
    #   3) iterate choosing random values within the partition.
    #
    # The downside is that we're skewing a _lot_ farther from uniformity than
    # we'd like.  Imagine we started at 0 with 1e18 partitions of size 100k
    # each.
    # Probability of '5' being returned =
    #   1.04e-22 = 1e-18 (chose first partition) * 1/9592 (chose '5')
    # Probability of '100003' being returned =
    #   1.19e-22 = 1e-18 (chose second partition) * 1/8392 (chose '100003')
    # Probability of '99999999999999999999977' being returned =
    #   5.20e-22 = 1e-18 (chose last partition)  *  1/1922 (chose '99...77')
    # So the primes in the last partition will show up 5x more often.
    # The partitions are selected uniformly, and the primes within are selected
    # uniformly, but the number of primes in each bucket is _not_ uniform.
    # Their individual probability of being selected is the probability of the
    # partition (uniform) times the probability of being selected inside the
    # partition (uniform with respect to all other primes in the same
    # partition, but each partition is different and skewed).
    #
    # Partitions are typically much larger than 100k, but with a huge range
    # we still see this (e.g. ~3x from 0-10^30, ~10x from 0-10^100).
    #
    # When selecting n-bit or n-digit primes, this effect is MUCH smaller, as
    # the skew becomes approx lg(2^n) / lg(2^(n-1)) which is pretty close to 1.
    #
    #
    # Another idea I'd like to try sometime is:
    #  pclo = prime_count_lower(low);
    #  pchi = prime_count_upper(high);
    #  do {
    #    $nth = random selection between pclo and pchi
    #    $prguess = nth_prime_approx($nth);
    #  } while ($prguess >= low) && ($prguess <= high);
    #  monte carlo select a prime in $prguess-2**24 to $prguess+2**24
    # which accounts for the prime distribution.

    my($binsize, $nparts);
    my $rand_part_size = 1 << (MPU_64BIT ? 32 : 31);
    if (ref($oddrange) eq 'Math::BigInt') {
      # Go to some trouble here because some systems are wonky, such as
      # giving us +a/+b = -r.  Also note the quotes for the bigint argument.
      # Without that, Math::BigInt::GMP can return garbage.
      my($nbins, $rem);
      ($nbins, $rem) = $oddrange->copy->bdiv( "$rand_part_size" );
      $nbins++ if $rem > 0;
      $nbins = $nbins->as_int();
      ($binsize,$rem) = $oddrange->copy->bdiv($nbins);
      $binsize++ if $rem > 0;
      $binsize = $binsize->as_int();
      $nparts  = $oddrange->copy->bdiv($binsize)->as_int();
      $low = $high->copy->bzero->badd($low) if ref($low) ne 'Math::BigInt';
    } else {
      my $nbins = int($oddrange / $rand_part_size);
      $nbins++ if $nbins * $rand_part_size != $oddrange;
      $binsize = int($oddrange / $nbins);
      $binsize++ if $binsize * $nbins != $oddrange;
      $nparts = int($oddrange/$binsize);
    }
    $nparts-- if ($nparts * $binsize) == $oddrange;

    my $rpart = urandomm($nparts+1);

    my $primelow = $low + 2 * $binsize * $rpart;
    my $partsize = ($rpart < $nparts) ? $binsize
                                      : $oddrange - ($nparts * $binsize);
    $partsize = _bigint_to_int($partsize) if ref($partsize) eq 'Math::BigInt';
    #warn "range $oddrange  = $nparts * $binsize + ", $oddrange - ($nparts * $binsize), "\n";
    #warn "  chose part $rpart size $partsize\n";
    #warn "  primelow is $low + 2 * $binsize * $rpart = $primelow\n";
    #die "Result could be too large" if ($primelow + 2*($partsize-1)) > $high;

    # Generate random numbers in the interval until one is prime.
    my $loop_limit = 2000 * 1000;  # To protect against broken rand

    # Simply things for non-bigints.
    if (ref($low) ne 'Math::BigInt') {
      while ($loop_limit-- > 0) {
        my $rand = urandomm($partsize);
        $prime = $primelow + $rand + $rand;
        croak "random prime failure, $prime > $high" if $prime > $high;
        if ($prime <= 23) {
          $prime = 2 if $prime == 1;  # special case for low = 2
          next unless (0,0,1,1,0,1,0,1,0,0,0,1,0,1,0,0,0,1,0,1,0,0,0,1)[$prime];
          return $prime;
        }
        next if !($prime % 3) || !($prime % 5) || !($prime % 7) || !($prime % 11);
        # It looks promising.  Check it.
        next unless is_prob_prime($prime);
        return $prime;
      }
      croak "Random function broken?";
    }

    # By checking a wheel 30 mod, we can skip anything that would be a multiple
    # of 2, 3, or 5, without even having to create the bigint prime.
    my @w30 = (1,0,5,4,3,2,1,0,3,2,1,0,1,0,3,2,1,0,1,0,3,2,1,0,5,4,3,2,1,0);
    my $primelow30 = $primelow % 30;
    $primelow30 = _bigint_to_int($primelow30) if ref($primelow30) eq 'Math::BigInt';

    # Big GCD's are hugely fast with GMP or Pari, but super slow with Calc.
    _make_big_gcds() if $_big_gcd_use < 0;

    while ($loop_limit-- > 0) {
      my $rand = urandomm($partsize);
      # Check wheel-30 mod
      my $rand30 = $rand % 30;
      next if $w30[($primelow30 + 2*$rand30) % 30]
              && ($rand > 3 || $primelow > 5);
      # Construct prime
      $prime = $primelow + $rand + $rand;
      croak "random prime failure, $prime > $high" if $prime > $high;
      if ($prime <= 23) {
        $prime = 2 if $prime == 1;  # special case for low = 2
        next unless (0,0,1,1,0,1,0,1,0,0,0,1,0,1,0,0,0,1,0,1,0,0,0,1)[$prime];
        return $prime;
      }
      # With GMP, the fastest thing to do is check primality.
      if (MPU_USE_GMP) {
        next unless Math::Prime::Util::GMP::is_prime($prime);
        return $prime;
      }
      # No MPU:GMP, so primality checking is slow.  Skip some composites here.
      next unless Math::BigInt::bgcd($prime, 7436429) == 1;
      if ($_big_gcd_use && $prime > $_big_gcd_top) {
        next unless Math::BigInt::bgcd($prime, $_big_gcd[0]) == 1;
        next unless Math::BigInt::bgcd($prime, $_big_gcd[1]) == 1;
        next unless Math::BigInt::bgcd($prime, $_big_gcd[2]) == 1;
        next unless Math::BigInt::bgcd($prime, $_big_gcd[3]) == 1;
      }
      # It looks promising.  Check it.
      next unless is_prob_prime($prime);
      return $prime;
    }
    croak "Random function broken?";
};

# Cache of tight bounds for each digit.  Helps performance a lot.
my @_random_ndigit_ranges = (undef, [2,7], [11,97] );
my @_random_nbit_ranges   = (undef, undef, [2,3],[5,7] );
my %_random_cache_small;

# For fixed small ranges with XS, e.g. 6-digit, 18-bit
sub _random_xscount_prime {
  my($low,$high) = @_;
  my($istart, $irange);
  my $cachearef = $_random_cache_small{$low,$high};
  if (defined $cachearef) {
    ($istart, $irange) = @$cachearef;
  } else {
    my $beg = ($low <= 2)  ?  2  :  next_prime($low-1);
    my $end = ($high < ~0)  ?  prev_prime($high + 1)  :  prev_prime($high);
    ($istart, $irange) = ( prime_count(2, $beg), prime_count($beg, $end) );
    $_random_cache_small{$low,$high} = [$istart, $irange];
  }
  my $rand = urandomm($irange);
  return nth_prime($istart + $rand);
}

sub random_prime {
  my($low,$high) = @_;
  return if $high < 2 || $low > $high;

  if ($high-$low > 1000000000) {
    # Range is large, just make them odd if needed.
    $low = 2 if $low < 2;
    $low++ if $low > 2 && ($low % 2) == 0;
    $high-- if ($high % 2) == 0;
  } else {
    # Tighten the range to the nearest prime.
    $low = ($low <= 2)  ?  2  :  next_prime($low-1);
    $high = ($high == ~0) ? prev_prime($high) : prev_prime($high + 1);
    return $low if ($low == $high) && is_prob_prime($low);
    return if $low >= $high;
    # At this point low and high are both primes, and low < high.
  }

  # At this point low and high are both primes, and low < high.
  return $_random_prime->($low, $high);
}

sub random_ndigit_prime {
  my($digits) = @_;
  croak "random_ndigit_prime, digits must be >= 1" unless $digits >= 1;

  return _random_xscount_prime( int(10 ** ($digits-1)), int(10 ** $digits) )
    if $digits <= 6 && MPU_USE_XS;

  my $bigdigits = $digits >= MPU_MAXDIGITS;
  if ($bigdigits && prime_get_config->{'nobigint'}) {
    croak "random_ndigit_prime with -nobigint, digits out of range"
      if $digits > MPU_MAXDIGITS;
    # Special case for nobigint and threshold digits
    if (!defined $_random_ndigit_ranges[$digits]) {
      my $low  = int(10 ** ($digits-1));
      my $high = ~0;
      $_random_ndigit_ranges[$digits] = [next_prime($low),prev_prime($high)];
    }
  }

  if (!defined $_random_ndigit_ranges[$digits]) {
    if ($bigdigits) {
      my $low  = Math::BigInt->new('10')->bpow($digits-1);
      my $high = Math::BigInt->new('10')->bpow($digits);
      # Just pull the range in to the nearest odd.
      $_random_ndigit_ranges[$digits] = [$low+1, $high-1];
    } else {
      my $low  = int(10 ** ($digits-1));
      my $high = int(10 ** $digits);
      # Note: Perl 5.6.2 cannot represent 10**15 as an integer, so things
      # will crash all over the place if you try.  We can stringify it, but
      # will just fail tests later.
      $_random_ndigit_ranges[$digits] = [next_prime($low),prev_prime($high)];
    }
  }
  my ($low, $high) = @{$_random_ndigit_ranges[$digits]};
  return $_random_prime->($low, $high);
}

my @_random_nbit_m;
my @_random_nbit_lambda;
my @_random_nbit_arange;

sub random_nbit_prime {
  my($bits) = @_;
  croak "random_nbit_prime, bits must be >= 2" unless $bits >= 2;
  $bits = int("$bits");

  # Very small size, use the nth-prime method
  if ($bits <= 20 && MPU_USE_XS) {
    if ($bits <= 4) {
      return (2,3)[urandomb(1)] if $bits == 2;
      return (5,7)[urandomb(1)] if $bits == 3;
      return (11,13)[urandomb(1)] if $bits == 4;
    }
    return _random_xscount_prime( 1 << ($bits-1), 1 << $bits );
  }

  croak "Mid-size random primes not supported on broken old Perl"
    if OLD_PERL_VERSION && MPU_64BIT && $bits > 49 && $bits <= 64;

  # Fouque and Tibouchi (2011) Algorithm 1 (basic)
  # Modified to make sure the nth bit is always set.
  #
  # Example for random_nbit_prime(512) on 64-bit Perl:
  # p:  1aaaaaaaabbbbbbbbbbbbbbbbbbbb1
  #     ^^       ^                   ^--- Trailing 1 so p is odd
  #     ||       +--- 512-63-2 = 447 lower bits selected before loop
  #     |+--- 63 upper bits selected in loop, repeated until p is prime
  #     +--- upper bit is 1 so we generate an n-bit prime
  # total: 1 + 63 + 447 + 1 = 512 bits
  #
  # Algorithm 2 is implemented in a previous commit on github.  The problem
  # is that it doesn't set the nth bit, and making that change requires a
  # modification of the algorithm.  It was not a lot faster than this A1
  # with the native int trial division.  If the irandf function was very
  # slow, then A2 would look more promising.
  #
  if (1 && $bits > 64) {
    my $l = (MPU_64BIT && $bits > 79)  ?  63  :  31;
    $l = 49 if $l == 63 && OLD_PERL_VERSION;  # Fix for broken Perl 5.6
    $l = $bits-2 if $bits-2 < $l;

    my $brand = urandomb($bits-$l-2);
    $brand = Math::BigInt->new("$brand") unless ref($brand) eq 'Math::BigInt';
    my $b = $brand->blsft(1)->binc();

    # Precalculate some modulii so we can do trial division on native int
    # 9699690 = 2*3*5*7*11*13*17*19, so later operations can be native ints
    my @premod;
    my $bpremod = _bigint_to_int($b->copy->bmod(9699690));
    my $twopremod = _bigint_to_int(Math::BigInt->new(2)->bmodpow($bits-$l-1, 9699690));
    foreach my $zi (0 .. 19-1) {
      foreach my $pm (3, 5, 7, 11, 13, 17, 19) {
        next if $zi >= $pm || defined $premod[$pm];
        $premod[$pm] = $zi if ( ($twopremod*$zi+$bpremod) % $pm) == 0;
      }
    }
    _make_big_gcds() if $_big_gcd_use < 0;
    if (!MPU_USE_GMP) { require Math::Prime::Util::PP; }

    my $loop_limit = 1_000_000;
    while ($loop_limit-- > 0) {
      my $a = (1 << $l) + urandomb($l);
      # $a % s == $premod[s]  =>  $p % s == 0  =>  p will be composite
      next if $a %  3 == $premod[ 3] || $a %  5 == $premod[ 5]
           || $a %  7 == $premod[ 7] || $a % 11 == $premod[11]
           || $a % 13 == $premod[13] || $a % 17 == $premod[17]
           || $a % 19 == $premod[19];
      my $p = Math::BigInt->new("$a")->blsft($bits-$l-1)->badd($b);
      #die " $a $b $p" if $a % 11 == $premod[11] && $p % 11 != 0;
      #die "!$a $b $p" if $a % 11 != $premod[11] && $p % 11 == 0;
      if (MPU_USE_GMP) {
        next unless Math::Prime::Util::GMP::is_prime($p);
      } else {
        next unless Math::BigInt::bgcd($p, 1348781387) == 1; # 23-43
        if ($_big_gcd_use && $p > $_big_gcd_top) {
          next unless Math::BigInt::bgcd($p, $_big_gcd[0]) == 1;
          next unless Math::BigInt::bgcd($p, $_big_gcd[1]) == 1;
          next unless Math::BigInt::bgcd($p, $_big_gcd[2]) == 1;
          next unless Math::BigInt::bgcd($p, $_big_gcd[3]) == 1;
        }
        # We know we don't have GMP and are > 2^64, so go directly to this.
        next unless Math::Prime::Util::PP::is_bpsw_prime($p);
      }
      return $p;
    }
    croak "Random function broken?";
  }

  # The Trivial method.  Great uniformity, and fine for small sizes.  It
  # gets very slow as the bit size increases, but that is why we have the
  # method above for bigints.
  if (1) {

    my $loop_limit = 2_000_000;
    if ($bits > MPU_MAXBITS) {
      my $p = Math::BigInt->bone->blsft($bits-1)->binc();
      while ($loop_limit-- > 0) {
        my $n = Math::BigInt->new(''.urandomb($bits-2))->blsft(1)->badd($p);
        return $n if is_prob_prime($n);
      }
    } else {
      my $p = (1 << ($bits-1)) + 1;
      while ($loop_limit-- > 0) {
        my $n = $p + (urandomb($bits-2) << 1);
        return $n if is_prob_prime($n);
      }
    }
    croak "Random function broken?";

  } else {

    # Send through the generic random_prime function.  Decently fast, but
    # quite a bit slower than the F&T A1 method above.
    if (!defined $_random_nbit_ranges[$bits]) {
      if ($bits > MPU_MAXBITS) {
        my $low  = Math::BigInt->new('2')->bpow($bits-1);
        my $high = Math::BigInt->new('2')->bpow($bits);
        # Don't pull the range in to primes, just odds
        $_random_nbit_ranges[$bits] = [$low+1, $high-1];
      } else {
        my $low  = 1 << ($bits-1);
        my $high = ($bits == MPU_MAXBITS)
                   ? ~0-1
                   : ~0 >> (MPU_MAXBITS - $bits);
        $_random_nbit_ranges[$bits] = [next_prime($low-1),prev_prime($high+1)];
        # Example: bits = 7.
        #    low = 1<<6 = 64.            next_prime(64-1)  = 67
        #    high = ~0 >> (64-7) = 127.  prev_prime(127+1) = 127
      }
    }
    my ($low, $high) = @{$_random_nbit_ranges[$bits]};
    return $_random_prime->($low, $high);

  }
}


# For stripping off the header on certificates so they can be combined.
sub _strip_proof_header {
  my $proof = shift;
  $proof =~ s/^\[MPU - Primality Certificate\]\nVersion \S+\n+Proof for:\nN (\d+)\n+//ms;
  return $proof;
}


sub random_maurer_prime {
  my $k = shift;
  croak "random_maurer_prime, bits must be >= 2" unless $k >= 2;
  $k = int("$k");

  return random_nbit_prime($k)  if $k <= MPU_MAXBITS && !OLD_PERL_VERSION;

  my ($n, $cert) = random_maurer_prime_with_cert($k);
  croak "maurer prime $n failed certificate verification!"
        unless verify_prime($cert);
  return $n;
}

sub random_maurer_prime_with_cert {
  my $k = shift;
  croak "random_maurer_prime, bits must be >= 2" unless $k >= 2;
  $k = int("$k");

  # This should never happen.  Trap now to prevent infinite loop.
  croak "number of bits must not be a bigint" if ref($k) eq 'Math::BigInt';

  # Results for random_nbit_prime are proven for all native bit sizes.
  my $p0 = MPU_MAXBITS;
  $p0 = 49 if OLD_PERL_VERSION && MPU_MAXBITS > 49;

  if ($k <= $p0) {
    my $n = random_nbit_prime($k);
    my ($isp, $cert) = is_provable_prime_with_cert($n);
    croak "small nbit prime could not be proven" if $isp != 2;
    return ($n, $cert);
  }

  # Set verbose to 3 to get pretty output like Crypt::Primes
  my $verbose = prime_get_config->{'verbose'};
  local $| = 1 if $verbose > 2;

  do { require Math::BigFloat; Math::BigFloat->import(); }
    if !defined $Math::BigFloat::VERSION;

  # Ignore Maurer's g and c that controls how much trial division is done.
  my $r = Math::BigFloat->new("0.5");   # relative size of the prime q
  my $m = 20;                           # makes sure R is big enough

  # Generate a random prime q of size $r*$k, where $r >= 0.5.  Try to
  # cleverly select r to match the size of a typical random factor.
  if ($k > 2*$m) {
    do {
      my $s = Math::Prime::Util::drand();
      $r = Math::BigFloat->new(2)->bpow($s-1);
    } while ($k*$r >= $k-$m);
  }

  # I've seen +0, +1, and +2 here.  Maurer uses +0.  Menezes uses +1.
  # We can use +1 because we're using BLS75 theorem 3 later.
  my $smallk = int(($r * $k)->bfloor->bstr) + 1;
  my ($q, $qcert) = random_maurer_prime_with_cert($smallk);
  $q = Math::BigInt->new("$q") unless ref($q) eq 'Math::BigInt';
  my $I = Math::BigInt->new(2)->bpow($k-2)->bdiv($q)->bfloor->as_int();
  print "r = $r  k = $k  q = $q  I = $I\n" if $verbose && $verbose != 3;
  $qcert = ($q < Math::BigInt->new("18446744073709551615"))
           ? "" : _strip_proof_header($qcert);

  # Big GCD's are hugely fast with GMP or Pari, but super slow with Calc.
  _make_big_gcds() if $_big_gcd_use < 0;
  my $ONE = Math::BigInt->bone;
  my $TWO = $ONE->copy->binc;

  my $loop_limit = 1_000_000 + $k * 1_000;
  while ($loop_limit-- > 0) {
    # R is a random number between $I+1 and 2*$I
    #my $R = $I + 1 + urandomm( $I );
    my $R = $I->copy->binc->badd( urandomm($I) );
    #my $n = 2 * $R * $q + 1;
    my $nm1 = $TWO->copy->bmul($R)->bmul($q);
    my $n = $nm1->copy->binc;
    # We constructed a promising looking $n.  Now test it.
    print "." if $verbose > 2;
    if (MPU_USE_GMP) {
      # MPU::GMP::is_prob_prime has fast tests built in.
      next unless Math::Prime::Util::GMP::is_prob_prime($n);
    } else {
      # No GMP, so first do trial divisions, then a SPSP test.
      next unless Math::BigInt::bgcd($n, 111546435)->is_one;
      if ($_big_gcd_use && $n > $_big_gcd_top) {
        next unless Math::BigInt::bgcd($n, $_big_gcd[0])->is_one;
        next unless Math::BigInt::bgcd($n, $_big_gcd[1])->is_one;
        next unless Math::BigInt::bgcd($n, $_big_gcd[2])->is_one;
        next unless Math::BigInt::bgcd($n, $_big_gcd[3])->is_one;
      }
      print "+" if $verbose > 2;
      next unless is_strong_pseudoprime($n, 3);
    }
    print "*" if $verbose > 2;

    # We could pick a random generator by doing:
    #   Step 1: pick a random r
    #   Step 2: compute g = r^((n-1)/q) mod p
    #   Step 3: if g == 1, goto Step 1.
    # Note that n = 2*R*q+1, hence the exponent is 2*R.

    # We could set r = 0.3333 earlier, then use BLS75 theorem 5 here.
    # The chain would be shorter, requiring less overall work for
    # large inputs.  Maurer's paper discusses the idea.

    # Use BLS75 theorem 3.  This is easier and possibly faster than
    # BLS75 theorem 4 (Pocklington) used by Maurer's paper.

    # Check conditions -- these should be redundant.
    my $m = $TWO * $R;
    if (! ($q->is_odd && $q > 2 && $m > 0 &&
           $m * $q + $ONE == $n && $TWO*$q+$ONE > $n->copy->bsqrt()) ) {
      carp "Maurer prime failed BLS75 theorem 3 conditions.  Retry.";
      next;
    }
    # Find a suitable a.  Move on if one isn't found quickly.
    foreach my $trya (2, 3, 5, 7, 11, 13) {
      my $a = Math::BigInt->new($trya);
      # m/2 = R    (n-1)/2 = (2*R*q)/2 = R*q
      next unless $a->copy->bmodpow($R, $n) != $nm1;
      next unless $a->copy->bmodpow($R*$q, $n) == $nm1;
      print "($k)" if $verbose > 2;
      croak "Maurer prime $n=2*$R*$q+1 failed BPSW" unless is_prob_prime($n);
      my $cert = "[MPU - Primality Certificate]\nVersion 1.0\n\n" .
                 "Proof for:\nN $n\n\n" .
                 "Type BLS3\nN $n\nQ $q\nA $a\n" .
                 $qcert;
      return ($n, $cert);
    }
    # Didn't pass the selected a values.  Try another R.
  }
  croak "Failure in random_maurer_prime, could not find a prime\n";
} # End of random_maurer_prime


sub random_shawe_taylor_prime_with_cert {
  my $k = shift;

  my $seed = random_bytes(512/8);

  my($status,$prime,$prime_seed,$prime_gen_counter,$cert)
     = _ST_Random_prime($k, $seed);
  croak "Shawe-Taylor random prime failure" unless $status;
  croak "Shawe-Taylor random prime failure: prime $prime failed certificate verification!" unless verify_prime($cert);

  return ($prime,$cert);
}

sub _seed_plus_one {
    my($s) = @_;
    for (my $i = length($s)-1; $i >= 0; $i--) {
        vec($s, $i, 8)++;
        last unless vec($s, $i, 8) == 0;
    }
    return $s;
}

sub _ST_Random_prime {  # From FIPS 186-4
  my($k, $input_seed) = @_;
  croak "Shawe-Taylor random prime must have length >= 2"  if $k < 2;
  $k = int("$k");

  croak "Shawe-Taylor random prime, invalid input seed"
     unless defined $input_seed && length($input_seed) >= 32;

  if (!defined $Digest::SHA::VERSION) {
    eval { require Digest::SHA;
           my $version = $Digest::SHA::VERSION;
           $version =~ s/[^\d.]//g;
           $version >= 4.00; }
      or do { croak "Must have Digest::SHA 4.00 or later"; };
  }

  my $k2 = Math::BigInt->new(2)->bpow($k-1);

  if ($k < 33) {
    my $seed = $input_seed;
    my $prime_gen_counter = 0;
    my $kmask    = 0xFFFFFFFF >> (32-$k);    # Does the mod operation
    my $kstencil = (1 << ($k-1)) | 1;        # Sets high and low bits
    while (1) {
      my $seedp1 = _seed_plus_one($seed);
      my $cvec = Digest::SHA::sha256($seed) ^ Digest::SHA::sha256($seedp1);
      # my $c = Math::BigInt->from_hex('0x' . unpack("H*", $cvec));
      # $c = $k2 + ($c % $k2);
      # $c = (2 * ($c >> 1)) + 1;
      my($c) = unpack("N*", substr($cvec,-4,4));
      $c = ($c & $kmask) | $kstencil;
      $prime_gen_counter++;
      $seed = _seed_plus_one($seedp1);
      my ($isp, $cert) = is_provable_prime_with_cert($c);
      return (1,$c,$seed,$prime_gen_counter,$cert) if $isp;
      return (0,0,0,0) if $prime_gen_counter > 10000 + 16*$k;
    }
  }
  my($status,$c0,$seed,$prime_gen_counter,$cert)
     = _ST_Random_prime( (($k+1)>>1)+1, $input_seed);
  return (0,0,0,0) unless $status;
  $cert = ($c0 < Math::BigInt->new("18446744073709551615"))
          ? "" : _strip_proof_header($cert);
  my $iterations = int(($k + 255) / 256) - 1;  # SHA256 generates 256 bits
  my $old_counter = $prime_gen_counter;
  my $xstr = '';
  for my $i (0 .. $iterations) {
    $xstr = Digest::SHA::sha256_hex($seed) . $xstr;
    $seed = _seed_plus_one($seed);
  }
  my $x = Math::BigInt->from_hex('0x'.$xstr);
  $x = $k2 + ($x % $k2);
  my $t = ($x + 2*$c0 - 1) / (2*$c0);
  _make_big_gcds() if $_big_gcd_use < 0;
  while (1) {
    if (2*$t*$c0 + 1 > 2*$k2) { $t = ($k2 + 2*$c0 - 1) / (2*$c0); }
    my $c = 2*$t*$c0 + 1;
    $prime_gen_counter++;

    # Don't do the Pocklington check unless the candidate looks prime
    my $looks_prime = 0;
    if (MPU_USE_GMP) {
      # MPU::GMP::is_prob_prime has fast tests built in.
      $looks_prime = Math::Prime::Util::GMP::is_prob_prime($c);
    } else {
      # No GMP, so first do trial divisions, then a SPSP test.
      $looks_prime = Math::BigInt::bgcd($c, 111546435)->is_one;
      if ($looks_prime && $_big_gcd_use && $c > $_big_gcd_top) {
        $looks_prime = Math::BigInt::bgcd($c, $_big_gcd[0])->is_one &&
                       Math::BigInt::bgcd($c, $_big_gcd[1])->is_one &&
                       Math::BigInt::bgcd($c, $_big_gcd[2])->is_one &&
                       Math::BigInt::bgcd($c, $_big_gcd[3])->is_one;
      }
      $looks_prime = 0 if $looks_prime && !is_strong_pseudoprime($c, 3);
    }

    if ($looks_prime) {
      # We could use a in (2,3,5,7,11,13), but pedantically use FIPS 186-4.
      my $astr = '';
      for my $i (0 .. $iterations) {
        $astr = Digest::SHA::sha256_hex($seed) . $astr;
        $seed = _seed_plus_one($seed);
      }
      my $a = Math::BigInt->from_hex('0x'.$astr);
      $a = ($a % ($c-3)) + 2;
      my $z = $a->copy->bmodpow(2*$t,$c);
      if (Math::BigInt::bgcd($z-1,$c)->is_one && $z->copy->bmodpow($c0,$c)->is_one) {
        croak "Shawe-Taylor random prime failure at ($k): $c not prime"
          unless is_prob_prime($c);
        $cert = "[MPU - Primality Certificate]\nVersion 1.0\n\n" .
                 "Proof for:\nN $c\n\n" .
                 "Type Pocklington\nN $c\nQ $c0\nA $a\n" .
                 $cert;
        return (1, $c, $seed, $prime_gen_counter, $cert);
      }
    } else {
      # Update seed "as if" we performed the Pocklington check from FIPS 186-4
      for my $i (0 .. $iterations) {
        $seed = _seed_plus_one($seed);
      }
    }
    return (0,0,0,0) if $prime_gen_counter > 10000 + 16*$k + $old_counter;
    $t++;
  }
}


# Gordon's algorithm for generating a strong prime.
sub random_strong_prime {
  my $t = shift;
  croak "random_strong_prime, bits must be >= 128" unless $t >= 128;
  $t = int("$t");

  croak "Random strong primes must be >= 173 bits on old Perl"
    if OLD_PERL_VERSION && MPU_64BIT && $t < 173;

  my $l   = (($t+1) >> 1) - 2;
  my $lp  = int($t/2) - 20;
  my $lpp = $l - 20;
  while (1) {
    my $qp  = random_nbit_prime($lp);
    my $qpp = random_nbit_prime($lpp);
    $qp  = Math::BigInt->new("$qp")  unless ref($qp)  eq 'Math::BigInt';
    $qpp = Math::BigInt->new("$qpp") unless ref($qpp) eq 'Math::BigInt';
    my ($il, $rem) = Math::BigInt->new(2)->bpow($l-1)->bdec()->bdiv(2*$qpp);
    $il++ if $rem > 0;
    $il = $il->as_int();
    my $iu = Math::BigInt->new(2)->bpow($l)->bsub(2)->bdiv(2*$qpp)->as_int();
    my $istart = $il + urandomm($iu - $il + 1);
    for (my $i = $istart; $i <= $iu; $i++) {  # Search for q
      my $q = 2 * $i * $qpp + 1;
      next unless is_prob_prime($q);
      my $pp = $qp->copy->bmodpow($q-2, $q)->bmul(2)->bmul($qp)->bdec();
      my ($jl, $rem) = Math::BigInt->new(2)->bpow($t-1)->bsub($pp)->bdiv(2*$q*$qp);
      $jl++ if $rem > 0;
      $jl = $jl->as_int();
      my $ju = Math::BigInt->new(2)->bpow($t)->bdec()->bsub($pp)->bdiv(2*$q*$qp)->as_int();
      my $jstart = $jl + urandomm($ju - $jl + 1);
      for (my $j = $jstart; $j <= $ju; $j++) {  # Search for p
        my $p = $pp + 2 * $j * $q * $qp;
        return $p if is_prob_prime($p);
      }
    }
  }
}

sub random_proven_prime {
  my $k = shift;
  my ($n, $cert) = random_proven_prime_with_cert($k);
  croak "random_proven_prime $n failed certificate verification!"
        unless verify_prime($cert);
  return $n;
}

sub random_proven_prime_with_cert {
  my $k = shift;

  if (prime_get_config->{'gmp'} && $k <= 450) {
    my $n = random_nbit_prime($k);
    my ($isp, $cert) = is_provable_prime_with_cert($n);
    croak "small nbit prime could not be proven" if $isp != 2;
    return ($n, $cert);
  }
  return random_maurer_prime_with_cert($k);
}

1;

__END__


# ABSTRACT:  Generate random primes

=pod

=encoding utf8

=head1 NAME

Math::Prime::Util::RandomPrimes - Generate random primes


=head1 VERSION

Version 0.73


=head1 SYNOPSIS

=head1 DESCRIPTION

Routines to generate random primes, including constructing proven primes.


=head1 RANDOM PRIME FUNCTIONS

=head2 random_prime

Generate a random prime between C<low> and C<high>.  If given one argument,
C<low> will be 2.

=head2 random_ndigit_prime

Generate a random prime with C<n> digits.  C<n> must be at least 1.

=head2 random_nbit_prime

Generate a random prime with C<n> bits.  C<n> must be at least 2.

=head2 random_maurer_prime

Construct a random provable prime of C<n> bits using Maurer's FastPrime
algorithm.  C<n> must be at least 2.

=head2 random_maurer_prime_with_cert

Construct a random provable prime of C<n> bits using Maurer's FastPrime
algorithm.  C<n> must be at least 2.  Returns a list of two items: the
prime and the certificate.

=head2 random_shawe_taylor_prime

Construct a random provable prime of C<n> bits using Shawe-Taylor's
algorithm.  C<n> must be at least 2.  The implementation is from
FIPS 186-4 and uses SHA-256 with 512 bits of randomness.

=head2 random_shawe_taylor_prime_with_cert

Construct a random provable prime of C<n> bits using Shawe-Taylor's
algorithm.  C<n> must be at least 2.  Returns a list of two items: the
prime and the certificate.

=head2 random_strong_prime

Construct a random strong prime of C<n> bits.  C<n> must be at least 128.

=head2 random_proven_prime

Generate or construct a random provable prime of C<n> bits.  C<n> must
be at least 2.

=head2 random_proven_prime_with_cert

Generate or construct a random provable prime of C<n> bits.  C<n> must
be at least 2.  Returns a list of two items: the prime and the certificate.


=head1 SEE ALSO

L<Math::Prime::Util>

=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>


=head1 COPYRIGHT

Copyright 2012-2013 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
