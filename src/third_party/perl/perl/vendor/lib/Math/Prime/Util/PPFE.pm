package Math::Prime::Util::PPFE;
use strict;
use warnings;
use Math::Prime::Util::PP;
use Math::Prime::Util::Entropy;

# The PP front end, only loaded if XS is not used.
# It is intended to load directly into the MPU namespace.

package Math::Prime::Util;
use Carp qw/carp croak confess/;

*_validate_num = \&Math::Prime::Util::PP::_validate_num;
*_validate_integer = \&Math::Prime::Util::PP::_validate_integer;
*_prime_memfreeall = \&Math::Prime::Util::PP::_prime_memfreeall;
*prime_memfree  = \&Math::Prime::Util::PP::prime_memfree;
*prime_precalc  = \&Math::Prime::Util::PP::prime_precalc;

use Math::Prime::Util::ChaCha;
*_is_csprng_well_seeded = \&Math::Prime::Util::ChaCha::_is_csprng_well_seeded;
*_csrand = \&Math::Prime::Util::ChaCha::csrand;
*_srand = \&Math::Prime::Util::ChaCha::srand;
*random_bytes = \&Math::Prime::Util::ChaCha::random_bytes;
*irand = \&Math::Prime::Util::ChaCha::irand;
*irand64 = \&Math::Prime::Util::ChaCha::irand64;

sub srand {
  my($seed) = @_;
  croak "secure option set, manual seeding disabled" if prime_get_config()->{'secure'};
  if (!defined $seed) {
    my $nbytes = (~0 == 4294967295) ? 4 : 8;
    $seed = entropy_bytes( $nbytes );
    $seed = unpack(($nbytes==4) ? "L" : "Q", $seed);
  }
  Math::Prime::Util::GMP::seed_csprng(8,pack("LL",$seed))
    if $Math::Prime::Util::_GMPfunc{"seed_csprng"};
  Math::Prime::Util::_srand($seed);
}
sub csrand {
  my($seed) = @_;
  croak "secure option set, manual seeding disabled" if defined $seed && prime_get_config()->{'secure'};
  $seed = entropy_bytes( 64 ) unless defined $seed;
  Math::Prime::Util::GMP::seed_csprng(length($seed),$seed)
    if $Math::Prime::Util::_GMPfunc{"seed_csprng"};
  Math::Prime::Util::_csrand($seed);
  1; # Don't return the seed
}
sub entropy_bytes {
  my($bytes) = @_;
  croak "entropy_bytes: input must be integer bytes between 1 and 4294967295"
    if !defined($bytes) || $bytes < 1 || $bytes > 4294967295 || $bytes != int($bytes);
  my $data = Math::Prime::Util::Entropy::entropy_bytes($bytes);
  if (!defined $data) {
    # We can't find any entropy source!  Highly unusual.
    Math::Prime::Util::_srand();
    $data = random_bytes($bytes);
  }
  croak "entropy_bytes internal got wrong amount!" unless length($data) == $bytes;
  $data;
}

# Fill all the mantissa bits for our NV, regardless of 32-bit or 64-bit Perl.
{
  use Config;
  my $nvbits = (defined $Config{nvmantbits})  ? $Config{nvmantbits}
             : (defined $Config{usequadmath}) ? 112
             : 53;
  my $uvbits = (~0 > 4294967295) ? 64 : 32;
  my $rsub;
  my $_tonv_32  = 1.0;        $_tonv_32 /= 2.0 for 1..32;
  my $_tonv_64  = $_tonv_32;  $_tonv_64 /= 2.0 for 1..32;
  my $_tonv_96  = $_tonv_64;  $_tonv_96 /= 2.0 for 1..32;
  my $_tonv_128 = $_tonv_96;  $_tonv_128/= 2.0 for 1..32;
  if ($uvbits == 64) {
    if ($nvbits <= 32) {
      *drand = sub { my $d = irand() * $_tonv_32;  $d *= $_[0] if $_[0];  $d; };
    } elsif ($nvbits <= 64) {
      *drand = sub { my $d = irand64() * $_tonv_64;  $d *= $_[0] if $_[0];  $d; };
    } else {
      *drand = sub { my $d = irand64() * $_tonv_64 + irand64() * $_tonv_128;  $d *= $_[0] if $_[0];  $d; };
    }
  } else {
    if ($nvbits <= 32) {
      *drand = sub { my $d = irand() * $_tonv_32;  $d *= $_[0] if $_[0];  $d; };
    } elsif ($nvbits <= 64) {
      *drand = sub { my $d = ((irand() >> 5) * 67108864.0 + (irand() >> 6)) / 9007199254740992.0;  $d *= $_[0] if $_[0];  $d; };
    } else {
      *drand = sub { my $d = irand() * $_tonv_32 + irand() * $_tonv_64 + irand() * $_tonv_96 + irand() * $_tonv_128;  $d *= $_[0] if $_[0];  $d; };
    }
  }
  *rand = \&drand;
}


*urandomb = \&Math::Prime::Util::PP::urandomb;
*urandomm = \&Math::Prime::Util::PP::urandomm;

# TODO: Go through these and decide if they should be doing anything extra here,
#       such as input validation.
# TODO: If not, why not the other functions?
*sumdigits = \&Math::Prime::Util::PP::sumdigits;
*todigits = \&Math::Prime::Util::PP::todigits;
*todigitstring = \&Math::Prime::Util::PP::todigitstring;
*fromdigits = \&Math::Prime::Util::PP::fromdigits;
*inverse_li = \&Math::Prime::Util::PP::inverse_li;
*sieve_prime_cluster = \&Math::Prime::Util::PP::sieve_prime_cluster;
*twin_prime_count = \&Math::Prime::Util::PP::twin_prime_count;
*semiprime_count = \&Math::Prime::Util::PP::semiprime_count;
*ramanujan_prime_count = \&Math::Prime::Util::PP::ramanujan_prime_count;
*sum_primes = \&Math::Prime::Util::PP::sum_primes;
*print_primes = \&Math::Prime::Util::PP::print_primes;
*sieve_range = \&Math::Prime::Util::PP::sieve_range;
*is_carmichael = \&Math::Prime::Util::PP::is_carmichael;
*is_quasi_carmichael = \&Math::Prime::Util::PP::is_quasi_carmichael;
*is_pillai = \&Math::Prime::Util::PP::is_pillai;
*is_fundamental = \&Math::Prime::Util::PP::is_fundamental;
*is_semiprime = \&Math::Prime::Util::PP::is_semiprime;
*is_totient = \&Math::Prime::Util::PP::is_totient;
*is_square = \&Math::Prime::Util::PP::is_square;

*random_prime = \&Math::Prime::Util::PP::random_prime;
*random_ndigit_prime = \&Math::Prime::Util::PP::random_ndigit_prime;
*random_nbit_prime = \&Math::Prime::Util::PP::random_nbit_prime;
*random_proven_prime = \&Math::Prime::Util::PP::random_maurer_prime; # redir
*random_strong_prime = \&Math::Prime::Util::PP::random_strong_prime;
*random_maurer_prime = \&Math::Prime::Util::PP::random_maurer_prime;
*random_shawe_taylor_prime =\&Math::Prime::Util::PP::random_shawe_taylor_prime;
*miller_rabin_random = \&Math::Prime::Util::PP::miller_rabin_random;
*random_semiprime = \&Math::Prime::Util::PP::random_semiprime;
*random_unrestricted_semiprime = \&Math::Prime::Util::PP::random_unrestricted_semiprime;
*random_factored_integer = \&Math::Prime::Util::PP::random_factored_integer;

*numtoperm = \&Math::Prime::Util::PP::numtoperm;
*permtonum = \&Math::Prime::Util::PP::permtonum;
*randperm = \&Math::Prime::Util::PP::randperm;
*shuffle = \&Math::Prime::Util::PP::shuffle;

*moebius = \&Math::Prime::Util::PP::moebius;
*euler_phi = \&Math::Prime::Util::PP::euler_phi;
*inverse_totient = \&Math::Prime::Util::PP::inverse_totient;

sub jordan_totient {
  my($k, $n) = @_;
  _validate_positive_integer($k);
  return 0 if defined $n && $n < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::jordan_totient($k, $n);
}
sub ramanujan_sum {
  my($k,$n) = @_;
  _validate_positive_integer($k);
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::ramanujan_sum($k, $n);
}
sub carmichael_lambda {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::carmichael_lambda($n);
}
sub mertens {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::mertens($n);
}
sub liouville {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::liouville($n);
}
sub exp_mangoldt {
  my($n) = @_;
  return 1 if defined $n && $n <= 1;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::exp_mangoldt($n);
}
sub hclassno {
  my($n) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::hclassno($n);
}


sub next_prime {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::next_prime($n);
}
sub prev_prime {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::prev_prime($n);
}
sub nth_prime {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_prime($n);
}
sub nth_prime_lower {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_prime_lower($n);
}
sub nth_prime_upper {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_prime_upper($n);
}
sub nth_prime_approx {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_prime_approx($n);
}
sub prime_count_lower {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::prime_count_lower($n);
}
sub prime_count_upper {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::prime_count_upper($n);
}
sub prime_count_approx {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::prime_count_approx($n);
}
sub twin_prime_count_approx {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::twin_prime_count_approx($n);
}
sub semiprime_count_approx {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::semiprime_count_approx($n);
}
sub ramanujan_prime_count_lower {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::ramanujan_prime_count_lower($n);
}
sub ramanujan_prime_count_upper {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::ramanujan_prime_count_upper($n);
}
sub ramanujan_prime_count_approx {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::ramanujan_prime_count_approx($n);
}
sub nth_twin_prime {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_twin_prime($n);
}
sub nth_twin_prime_approx {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_twin_prime_approx($n);
}
sub nth_semiprime {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_semiprime($n);
}
sub nth_semiprime_approx {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_semiprime_approx($n);
}
sub nth_ramanujan_prime {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_ramanujan_prime($n);
}
sub nth_ramanujan_prime_lower {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_ramanujan_prime_lower($n);
}
sub nth_ramanujan_prime_upper {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_ramanujan_prime_upper($n);
}
sub nth_ramanujan_prime_approx {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::nth_ramanujan_prime_approx($n);
}


*is_prime          = \&Math::Prime::Util::PP::is_prime;
*is_prob_prime     = \&Math::Prime::Util::PP::is_prob_prime;
*is_provable_prime = \&Math::Prime::Util::PP::is_provable_prime;
*is_bpsw_prime     = \&Math::Prime::Util::PP::is_bpsw_prime;

sub is_pseudoprime {
  my($n, @bases) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  croak "No bases given to is_strong_pseudoprime" unless @bases;
  return Math::Prime::Util::PP::is_pseudoprime($n, @bases);
}
sub is_euler_pseudoprime {
  my($n, @bases) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  croak "No bases given to is_euler_pseudoprime" unless @bases;
  return Math::Prime::Util::PP::is_euler_pseudoprime($n, @bases);
}
sub is_strong_pseudoprime {
  my($n, @bases) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  croak "No bases given to is_strong_pseudoprime" unless @bases;
  return Math::Prime::Util::PP::is_strong_pseudoprime($n, @bases);
}
sub is_euler_plumb_pseudoprime {
  my($n) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::is_euler_plumb_pseudoprime($n);
}
sub is_lucas_pseudoprime {
  my($n) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::is_lucas_pseudoprime($n);
}
sub is_strong_lucas_pseudoprime {
  my($n) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::is_strong_lucas_pseudoprime($n);
}
sub is_extra_strong_lucas_pseudoprime {
  my($n) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::is_extra_strong_lucas_pseudoprime($n);
}
sub is_almost_extra_strong_lucas_pseudoprime {
  my($n, $increment) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  if (defined $increment) { _validate_positive_integer($increment, 1, 256);
  } else                  { $increment = 1; }
  return Math::Prime::Util::PP::is_almost_extra_strong_lucas_pseudoprime($n, $increment);
}
sub is_perrin_pseudoprime {
  my($n,$restrict) = @_;
  return 0 if defined $n && int($n) < 0;
  $restrict = 0 unless defined $restrict;
  _validate_positive_integer($n);
  _validate_positive_integer($restrict);
  return Math::Prime::Util::PP::is_perrin_pseudoprime($n, $restrict);
}
sub is_catalan_pseudoprime {
  my($n) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::is_catalan_pseudoprime($n);
}
sub is_frobenius_pseudoprime {
  my($n, $P, $Q) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  # TODO: validate P & Q
  return Math::Prime::Util::PP::is_frobenius_pseudoprime($n, $P, $Q);
}
sub is_frobenius_underwood_pseudoprime {
  my($n) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::is_frobenius_underwood_pseudoprime($n);
}
sub is_frobenius_khashin_pseudoprime {
  my($n) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::is_frobenius_khashin_pseudoprime($n);
}
sub is_aks_prime {
  my($n) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::is_aks_prime($n);
}
sub is_ramanujan_prime {
  my($n) = @_;
  return 0 if defined $n && int($n) < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::is_ramanujan_prime($n);
}
sub is_mersenne_prime {
  my($p) = @_;
  _validate_positive_integer($p);
  return Math::Prime::Util::PP::is_mersenne_prime($p);
}
sub is_square_free {
  my($n) = @_;
  _validate_integer($n);
  return Math::Prime::Util::PP::is_square_free($n);
}
sub is_primitive_root {
  my($a,$n) = @_;
  return 0 if $n == 0;
  $n = -$n if defined $n && $n < 0;
  $a %= $n if defined $a && $a < 0;
  _validate_positive_integer($a);
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::is_primitive_root($a,$n);
}


sub lucas_sequence {
  my($n, $P, $Q, $k) = @_;
  my ($vp, $vq) = ($P, $Q);
  $vp = -$vp if defined $vp && $vp < 0;
  $vq = -$vq if defined $vq && $vq < 0;
  _validate_positive_integer($n);
  _validate_positive_integer($vp);
  _validate_positive_integer($vq);
  _validate_positive_integer($k);
  return Math::Prime::Util::PP::lucas_sequence(@_);
}
sub lucasu {
  my($P, $Q, $k) = @_;
  my ($vp, $vq) = ($P, $Q);
  $vp = -$vp if defined $vp && $vp < 0;
  $vq = -$vq if defined $vq && $vq < 0;
  _validate_positive_integer($vp);
  _validate_positive_integer($vq);
  _validate_positive_integer($k);
  return Math::Prime::Util::PP::lucasu(@_);
}
sub lucasv {
  my($P, $Q, $k) = @_;
  my ($vp, $vq) = ($P, $Q);
  $vp = -$vp if defined $vp && $vp < 0;
  $vq = -$vq if defined $vq && $vq < 0;
  _validate_positive_integer($vp);
  _validate_positive_integer($vq);
  _validate_positive_integer($k);
  return Math::Prime::Util::PP::lucasv(@_);
}

sub kronecker {
  my($a, $b) = @_;
  my ($va, $vb) = ($a, $b);
  $va = -$va if defined $va && $va < 0;
  $vb = -$vb if defined $vb && $vb < 0;
  _validate_positive_integer($va);
  _validate_positive_integer($vb);
  return Math::Prime::Util::PP::kronecker(@_);
}

sub factorial {
  my($n) = @_;
  _validate_integer($n);
  return Math::Prime::Util::PP::factorial($n);
}

sub factorialmod {
  my($n, $m) = @_;
  _validate_integer($n);
  _validate_integer($m);
  return Math::Prime::Util::PP::factorialmod($n, $m);
}

sub binomial {
  my($n, $k) = @_;
  _validate_integer($n);
  _validate_integer($k);
  return Math::Prime::Util::PP::binomial($n, $k);
}

sub stirling {
  my($n, $k, $type) = @_;
  _validate_positive_integer($n);
  _validate_positive_integer($k);
  _validate_positive_integer($type) if defined $type;
  return Math::Prime::Util::PP::stirling($n, $k, $type);
}

sub znorder {
  my($a, $n) = @_;
  _validate_positive_integer($a);
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::znorder($a, $n);
}

sub znlog {
  my($a, $g, $p) = @_;
  _validate_positive_integer($a);
  _validate_positive_integer($g);
  _validate_positive_integer($p);
  return Math::Prime::Util::PP::znlog($a, $g, $p);
}

sub znprimroot {
  my($n) = @_;
  $n =~ s/^-(\d+)/$1/ if defined $n;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::znprimroot($n);
}

sub trial_factor {
  my($n, $maxlim) = @_;
  _validate_positive_integer($n);
  if (defined $maxlim) {
    _validate_positive_integer($maxlim);
    return Math::Prime::Util::PP::trial_factor($n, $maxlim);
  }
  return Math::Prime::Util::PP::trial_factor($n);
}
sub fermat_factor {
  my($n, $rounds) = @_;
  _validate_positive_integer($n);
  if (defined $rounds) {
    _validate_positive_integer($rounds);
    return Math::Prime::Util::PP::fermat_factor($n, $rounds);
  }
  return Math::Prime::Util::PP::fermat_factor($n);
}
sub holf_factor {
  my($n, $rounds) = @_;
  _validate_positive_integer($n);
  if (defined $rounds) {
    _validate_positive_integer($rounds);
    return Math::Prime::Util::PP::holf_factor($n, $rounds);
  }
  return Math::Prime::Util::PP::holf_factor($n);
}
sub squfof_factor {
  my($n, $rounds) = @_;
  _validate_positive_integer($n);
  if (defined $rounds) {
    _validate_positive_integer($rounds);
    return Math::Prime::Util::PP::squfof_factor($n, $rounds);
  }
  return Math::Prime::Util::PP::squfof_factor($n);
}
sub pbrent_factor {
  my($n, $rounds, $pa) = @_;
  _validate_positive_integer($n);
  if (defined $rounds) { _validate_positive_integer($rounds);
  } else               { $rounds = 4*1024*1024; }
  if (defined $pa    ) { _validate_positive_integer($pa);
  } else               { $pa = 3; }
  return Math::Prime::Util::PP::pbrent_factor($n, $rounds, $pa);
}
sub prho_factor {
  my($n, $rounds, $pa) = @_;
  _validate_positive_integer($n);
  if (defined $rounds) { _validate_positive_integer($rounds);
  } else               { $rounds = 4*1024*1024; }
  if (defined $pa    ) { _validate_positive_integer($pa);
  } else               { $pa = 3; }
  return Math::Prime::Util::PP::prho_factor($n, $rounds, $pa);
}
sub pminus1_factor {
  my($n, $B1, $B2) = @_;
  _validate_positive_integer($n);
  _validate_positive_integer($B1) if defined $B1;
  _validate_positive_integer($B2) if defined $B2;
  Math::Prime::Util::PP::pminus1_factor($n, $B1, $B2);
}
*pplus1_factor = \&pminus1_factor;

sub divisors {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::divisors($n);
}

sub divisor_sum {
  my($n, $k) = @_;
  _validate_positive_integer($n);
  _validate_positive_integer($k) if defined $k && ref($k) ne 'CODE';
  return Math::Prime::Util::PP::divisor_sum($n, $k);
}

sub gcd {
  my(@v) = @_;
  _validate_integer($_) for @v;
  return Math::Prime::Util::PP::gcd(@v);
}
sub lcm {
  my(@v) = @_;
  _validate_integer($_) for @v;
  return Math::Prime::Util::PP::lcm(@v);
}
sub gcdext {
  my($a,$b) = @_;
  _validate_integer($a);
  _validate_integer($b);
  return Math::Prime::Util::PP::gcdext($a,$b);
}
sub chinese {
  # TODO: make sure we're not modding their data
  foreach my $aref (@_) {
    die "chinese arguments are two-element array references"
      unless ref($aref) eq 'ARRAY' && scalar @$aref == 2;
    _validate_integer($aref->[0]);
    _validate_integer($aref->[1]);
  }
  return Math::Prime::Util::PP::chinese(@_);
}
sub vecsum {
  my(@v) = @_;
  _validate_integer($_) for @v;
  return Math::Prime::Util::PP::vecsum(@v);
}
sub vecprod {
  my(@v) = @_;
  _validate_integer($_) for @v;
  return Math::Prime::Util::PP::vecprod(@v);
}
sub vecmin {
  my(@v) = @_;
  _validate_integer($_) for @v;
  return Math::Prime::Util::PP::vecmin(@v);
}
sub vecmax {
  my(@v) = @_;
  _validate_integer($_) for @v;
  return Math::Prime::Util::PP::vecmax(@v);
}
sub invmod {
  my ($a, $n) = @_;
  _validate_integer($a);
  _validate_integer($n);
  return Math::Prime::Util::PP::invmod($a,$n);
}
sub sqrtmod {
  my ($a, $n) = @_;
  _validate_integer($a);
  _validate_integer($n);
  return Math::Prime::Util::PP::sqrtmod($a,$n);
}
sub addmod {
  my ($a, $b, $n) = @_;
  _validate_integer($a); _validate_integer($b>=0?$b:-$b); _validate_integer($n);
  return Math::Prime::Util::PP::addmod($a,$b, $n);
}
sub mulmod {
  my ($a, $b, $n) = @_;
  _validate_integer($a); _validate_integer($b>=0?$b:-$b); _validate_integer($n);
  return Math::Prime::Util::PP::mulmod($a,$b, $n);
}
sub divmod {
  my ($a, $b, $n) = @_;
  _validate_integer($a); _validate_integer($b>=0?$b:-$b); _validate_integer($n);
  return Math::Prime::Util::PP::divmod($a,$b, $n);
}
sub powmod {
  my ($a, $b, $n) = @_;
  _validate_integer($a); _validate_integer($b>=0?$b:-$b); _validate_integer($n);
  return Math::Prime::Util::PP::powmod($a,$b, $n);
}
sub sqrtint {
  my($n) = @_;
  _validate_integer($n);
  return Math::Prime::Util::PP::sqrtint($n);
}
sub rootint {
  my($n, $k, $refp) = @_;
  _validate_positive_integer($n);
  _validate_positive_integer($k);
  return Math::Prime::Util::PP::rootint($n, $k, $refp);
}
sub logint {
  my($n, $b, $refp) = @_;
  _validate_positive_integer($n);
  _validate_positive_integer($b);
  return Math::Prime::Util::PP::logint($n, $b, $refp);
}

sub legendre_phi {
  my($x, $a) = @_;
  _validate_positive_integer($x);
  _validate_positive_integer($a);
  return Math::Prime::Util::PP::legendre_phi($x, $a);
}

sub chebyshev_theta {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::chebyshev_theta($n);
}
sub chebyshev_psi {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::chebyshev_psi($n);
}
sub ramanujan_tau {
  my($n) = @_;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::ramanujan_tau($n);
}

sub is_power {
  my($n, $a, $refp) = @_;
  my $vn = "$n";  $vn =~ s/^-//;
  _validate_positive_integer($vn);
  _validate_positive_integer($a) if defined $a;
  $vn = '-'.$vn if $n < 0;
  return Math::Prime::Util::PP::is_power($vn, $a, $refp);
}
sub is_prime_power {
  my($n, $refp) = @_;
  my $vn = "$n";  $vn =~ s/^-//;
  _validate_positive_integer($vn);
  $vn = '-'.$vn if $n < 0;
  return Math::Prime::Util::PP::is_prime_power($vn, $refp);
}
sub is_polygonal {
  my($x, $s, $refp) = @_;
  my $vx = "$x";  $vx =~ s/^-//;
  _validate_positive_integer($vx);
  _validate_positive_integer($s);
  $vx = '-'.$vx if $x < 0;
  return Math::Prime::Util::PP::is_polygonal($vx, $s, $refp);
}
sub valuation {
  my($n, $k) = @_;
  $n = -$n if defined $n && $n < 0;
  $k = -$k if defined $k && $k < 0;
  _validate_positive_integer($n);
  _validate_positive_integer($k);
  return Math::Prime::Util::PP::valuation($n, $k);
}
sub hammingweight {
  my($n) = @_;
  $n = -$n if defined $n && $n < 0;
  _validate_positive_integer($n);
  return Math::Prime::Util::PP::hammingweight($n);
}

sub Pi {
  my($digits) = @_;
  _validate_positive_integer($digits) if defined $digits;
  return Math::Prime::Util::PP::Pi($digits);
}

#############################################################################

my $_exitloop = 0;
sub lastfor { $_exitloop = 1; }
sub _get_forexit { $_exitloop; }
sub _start_for_loop { my $old = $_exitloop; $_exitloop = 0; $old; }
sub _end_for_loop { $_exitloop = shift; }

sub forprimes (&$;$) {    ## no critic qw(ProhibitSubroutinePrototypes)
  my($sub, $beg, $end) = @_;
  if (!defined $end) { $end = $beg; $beg = 2; }
  _validate_num($beg) || _validate_positive_integer($beg);
  _validate_num($end) || _validate_positive_integer($end);
  $beg = 2 if $beg < 2;
  my $oldforexit = _start_for_loop();
  {
    my $pp;
    local *_ = \$pp;
    for (my $p = next_prime($beg-1);  $p <= $end;  $p = next_prime($p)) {
      $pp = $p;
      $sub->();
      last if $_exitloop;
    }
  }
  _end_for_loop($oldforexit);
}

sub forcomposites(&$;$) { ## no critic qw(ProhibitSubroutinePrototypes)
  Math::Prime::Util::_generic_forcomp_sub('composites', @_);
}
sub foroddcomposites(&$;$) { ## no critic qw(ProhibitSubroutinePrototypes)
  Math::Prime::Util::_generic_forcomp_sub('oddcomposites', @_);
}
sub forsemiprimes(&$;$) { ## no critic qw(ProhibitSubroutinePrototypes)
  Math::Prime::Util::_generic_forcomp_sub('semiprimes', @_);
}

sub forfactored(&$;$) { ## no critic qw(ProhibitSubroutinePrototypes)
  Math::Prime::Util::_generic_forfac(0, @_);
}
sub forsquarefree(&$;$) { ## no critic qw(ProhibitSubroutinePrototypes)
  Math::Prime::Util::_generic_forfac(1, @_);
}

sub fordivisors (&$) {    ## no critic qw(ProhibitSubroutinePrototypes)
  my($sub, $n) = @_;
  _validate_num($n) || _validate_positive_integer($n);
  my @divisors = divisors($n);
  my $oldforexit = _start_for_loop();
  {
    my $pp;
    local *_ = \$pp;
    foreach my $d (@divisors) {
      $pp = $d;
      $sub->();
      last if $_exitloop;
    }
  }
  _end_for_loop($oldforexit);
}

sub forpart (&$;$) {    ## no critic qw(ProhibitSubroutinePrototypes)
  Math::Prime::Util::PP::forpart(@_);
}
sub forcomp (&$;$) {    ## no critic qw(ProhibitSubroutinePrototypes)
  Math::Prime::Util::PP::forcomp(@_);
}
sub forcomb (&$;$) {    ## no critic qw(ProhibitSubroutinePrototypes)
  Math::Prime::Util::PP::forcomb(@_);
}
sub forperm (&$;$) {    ## no critic qw(ProhibitSubroutinePrototypes)
  Math::Prime::Util::PP::forperm(@_);
}
sub forderange (&$;$) {    ## no critic qw(ProhibitSubroutinePrototypes)
  Math::Prime::Util::PP::forderange(@_);
}

sub forsetproduct (&@) {    ## no critic qw(ProhibitSubroutinePrototypes)
  my($sub, @v) = @_;
  croak 'Not a subroutine reference' unless (ref($sub) || '') eq 'CODE';
  croak 'Not an array reference' if grep {(ref($_) || '') ne 'ARRAY'} @v;
  # Exit if no arrays or any are empty.
  return if scalar(@v) == 0 || grep { !@$_ } @v;

  my @outv = map { $v[$_]->[0] } 0 .. $#v;
  my @cnt = (0) x @v;

  my $oldforexit = _start_for_loop();
  my $i = 0;
  while ($i >= 0) {
    $sub->(@outv);
    last if $_exitloop;
    for ($i = $#v; $i >= 0; $i--) {
      if ($cnt[$i] >= $#{$v[$i]}) { $cnt[$i] = 0; $outv[$i] = $v[$i]->[0]; }
      else { $outv[$i] = $v[$i]->[++$cnt[$i]]; last; }
    }
  }
  _end_for_loop($oldforexit);
}

sub vecreduce (&@) {    ## no critic qw(ProhibitSubroutinePrototypes)
  my($sub, @v) = @_;

  # Mastering Perl page 162, works with old Perl
  my $caller = caller();
  no strict 'refs'; ## no critic(strict)
  local(*{$caller.'::a'}) = \my $a;
  local(*{$caller.'::b'}) = \my $b;
  $a = shift @v;
  for my $v (@v) {
    $b = $v;
    $a = $sub->();
  }
  $a;
}

sub vecany (&@) {       ## no critic qw(ProhibitSubroutinePrototypes)
  my $sub = shift;
  $sub->() and return 1 foreach @_;
  0;
}
sub vecall (&@) {       ## no critic qw(ProhibitSubroutinePrototypes)
  my $sub = shift;
  $sub->() or return 0 foreach @_;
  1;
}
sub vecnone (&@) {      ## no critic qw(ProhibitSubroutinePrototypes)
  my $sub = shift;
  $sub->() and return 0 foreach @_;
  1;
}
sub vecnotall (&@) {    ## no critic qw(ProhibitSubroutinePrototypes)
  my $sub = shift;
  $sub->() or return 1 foreach @_;
  undef;
}

sub vecfirst (&@) {     ## no critic qw(ProhibitSubroutinePrototypes)
  my $sub = shift;
  $sub->() and return $_ foreach @_;
  undef;
}

sub vecfirstidx (&@) {     ## no critic qw(ProhibitSubroutinePrototypes)
  my $sub = shift;
  my $i = 0;
  ++$i and $sub->() and return $i-1 foreach @_;
  -1;
}

sub vecextract {
  my($aref, $mask) = @_;
  croak "vecextract first argument must be an array reference"
    unless ref($aref) eq 'ARRAY';
  return Math::Prime::Util::PP::vecextract(@_);
}

1;

__END__

=pod

=head1 NAME

Math::Prime::Util::PPFE - PP front end for Math::Prime::Util

=head1 SYNOPSIS

This loads the PP code and adds input validation front ends.  It is only
meant to be used when XS is not used.

=head1 DESCRIPTION

Loads PP module and implements PP front-end functions for all XS code.
This is used only if the XS code is not loaded.

=head1 SEE ALSO

L<Math::Prime::Util>

L<Math::Prime::Util::PP>

=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>


=head1 COPYRIGHT

Copyright 2014-2016 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
