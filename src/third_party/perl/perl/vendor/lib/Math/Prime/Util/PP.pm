package Math::Prime::Util::PP;
use strict;
use warnings;
use Carp qw/carp croak confess/;

BEGIN {
  $Math::Prime::Util::PP::AUTHORITY = 'cpan:DANAJ';
  $Math::Prime::Util::PP::VERSION = '0.73';
}

BEGIN {
  do { require Math::BigInt;  Math::BigInt->import(try=>"GMP,Pari"); }
    unless defined $Math::BigInt::VERSION;
}

# The Pure Perl versions of all the Math::Prime::Util routines.
#
# Some of these will be relatively similar in performance, some will be
# very slow in comparison.
#
# Most of these are pretty simple.  Also, you really should look at the C
# code for more detailed comments, including references to papers.

BEGIN {
  use constant OLD_PERL_VERSION=> $] < 5.008;
  use constant MPU_MAXBITS     => (~0 == 4294967295) ? 32 : 64;
  use constant MPU_64BIT       => MPU_MAXBITS == 64;
  use constant MPU_32BIT       => MPU_MAXBITS == 32;
 #use constant MPU_MAXPARAM    => MPU_32BIT ? 4294967295 : 18446744073709551615;
 #use constant MPU_MAXDIGITS   => MPU_32BIT ? 10 : 20;
  use constant MPU_MAXPRIME    => MPU_32BIT ? 4294967291 : 18446744073709551557;
  use constant MPU_MAXPRIMEIDX => MPU_32BIT ?  203280221 :  425656284035217743;
  use constant MPU_HALFWORD    => MPU_32BIT ? 65536 : OLD_PERL_VERSION ? 33554432 : 4294967296;
  use constant UVPACKLET       => MPU_32BIT ? 'L' : 'Q';
  use constant MPU_INFINITY    => (65535 > 0+'inf') ? 20**20**20 : 0+'inf';
  use constant BZERO           => Math::BigInt->bzero;
  use constant BONE            => Math::BigInt->bone;
  use constant BTWO            => Math::BigInt->new(2);
  use constant INTMAX          => (!OLD_PERL_VERSION || MPU_32BIT) ? ~0 : 562949953421312;
  use constant BMAX            => Math::BigInt->new('' . INTMAX);
  use constant B_PRIM767       => Math::BigInt->new("261944051702675568529303");
  use constant B_PRIM235       => Math::BigInt->new("30");
  use constant PI_TIMES_8      => 25.13274122871834590770114707;
}

my $_precalc_size = 0;
sub prime_precalc {
  my($n) = @_;
  croak "Parameter '$n' must be a positive integer" unless _is_positive_int($n);
  $_precalc_size = $n if $n > $_precalc_size;
}
sub prime_memfree {
  $_precalc_size = 0;
  eval { Math::Prime::Util::GMP::_GMP_memfree(); }
    if defined $Math::Prime::Util::GMP::VERSION && $Math::Prime::Util::GMP::VERSION >= 0.49;
}
sub _get_prime_cache_size { $_precalc_size }
sub _prime_memfreeall { prime_memfree; }


sub _is_positive_int {
  ((defined $_[0]) && $_[0] ne '' && ($_[0] !~ tr/0123456789//c));
}

sub _bigint_to_int {
  #if (OLD_PERL_VERSION) {
  #  my $pack = ($_[0] < 0) ? lc(UVPACKLET) : UVPACKLET;
  #  return unpack($pack,pack($pack,"$_[0]"));
  #}
  int("$_[0]");
}

sub _upgrade_to_float {
  do { require Math::BigFloat; Math::BigFloat->import(); }
    if !defined $Math::BigFloat::VERSION;
  Math::BigFloat->new(@_);
}

# Get the accuracy of variable x, or the max default from BigInt/BigFloat
# One might think to use ref($x)->accuracy() but numbers get upgraded and
# downgraded willy-nilly, and it will do the wrong thing from the user's
# perspective.
sub _find_big_acc {
  my($x) = @_;
  my $b;

  $b = $x->accuracy() if ref($x) =~ /^Math::Big/;
  return $b if defined $b;

  my ($i,$f) = (Math::BigInt->accuracy(), Math::BigFloat->accuracy());
  return (($i > $f) ? $i : $f)   if defined $i && defined $f;
  return $i if defined $i;
  return $f if defined $f;

  ($i,$f) = (Math::BigInt->div_scale(), Math::BigFloat->div_scale());
  return (($i > $f) ? $i : $f)   if defined $i && defined $f;
  return $i if defined $i;
  return $f if defined $f;
  return 18;
}

sub _bfdigits {
  my($wantbf, $xdigits) = (0, 17);
  if (defined $bignum::VERSION || ref($_[0]) =~ /^Math::Big/) {
    do { require Math::BigFloat; Math::BigFloat->import(); }
      if !defined $Math::BigFloat::VERSION;
    if (ref($_[0]) eq 'Math::BigInt') {
      my $xacc = ($_[0])->accuracy();
      $_[0] = Math::BigFloat->new($_[0]);
      ($_[0])->accuracy($xacc) if $xacc;
    }
    $_[0] = Math::BigFloat->new("$_[0]") if ref($_[0]) ne 'Math::BigFloat';
    $wantbf = _find_big_acc($_[0]);
    $xdigits = $wantbf;
  }
  ($wantbf, $xdigits);
}


sub _validate_num {
  my($n, $min, $max) = @_;
  croak "Parameter must be defined" if !defined $n;
  return 0 if ref($n);
  croak "Parameter '$n' must be a positive integer"
          if $n eq '' || ($n =~ tr/0123456789//c && $n !~ /^\+\d+$/);
  croak "Parameter '$n' must be >= $min" if defined $min && $n < $min;
  croak "Parameter '$n' must be <= $max" if defined $max && $n > $max;
  substr($_[0],0,1,'') if substr($n,0,1) eq '+';
  return 0 unless $n < ~0 || int($n) eq ''.~0;
  1;
}

sub _validate_positive_integer {
  my($n, $min, $max) = @_;
  croak "Parameter must be defined" if !defined $n;
  if (ref($n) eq 'CODE') {
    $_[0] = $_[0]->();
    $n = $_[0];
  }
  if (ref($n) eq 'Math::BigInt') {
    croak "Parameter '$n' must be a positive integer"
      if $n->sign() ne '+' || !$n->is_int();
    $_[0] = _bigint_to_int($_[0]) if $n <= BMAX;
  } elsif (ref($n) eq 'Math::GMPz') {
    croak "Parameter '$n' must be a positive integer" if Math::GMPz::Rmpz_sgn($n) < 0;
    $_[0] = _bigint_to_int($_[0]) if $n <= INTMAX;
  } else {
    my $strn = "$n";
    if ($strn eq '-0') { $_[0] = 0; $strn = '0'; }
    croak "Parameter '$strn' must be a positive integer"
      if $strn eq '' || ($strn =~ tr/0123456789//c && $strn !~ /^\+?\d+$/);
    if ($n <= INTMAX) {
      $_[0] = $strn if ref($n);
    } else {
      $_[0] = Math::BigInt->new($strn)
    }
  }
  $_[0]->upgrade(undef) if ref($_[0]) eq 'Math::BigInt' && $_[0]->upgrade();
  croak "Parameter '$_[0]' must be >= $min" if defined $min && $_[0] < $min;
  croak "Parameter '$_[0]' must be <= $max" if defined $max && $_[0] > $max;
  1;
}

sub _validate_integer {
  my($n) = @_;
  croak "Parameter must be defined" if !defined $n;
  if (ref($n) eq 'CODE') {
    $_[0] = $_[0]->();
    $n = $_[0];
  }
  my $poscmp = OLD_PERL_VERSION ?  562949953421312 : ''.~0;
  my $negcmp = OLD_PERL_VERSION ? -562949953421312 : -(~0 >> 1);
  if (ref($n) eq 'Math::BigInt') {
    croak "Parameter '$n' must be an integer" if !$n->is_int();
    $_[0] = _bigint_to_int($_[0]) if $n <= $poscmp && $n >= $negcmp;
  } else {
    my $strn = "$n";
    if ($strn eq '-0') { $_[0] = 0; $strn = '0'; }
    croak "Parameter '$strn' must be an integer"
      if $strn eq '' || ($strn =~ tr/-0123456789//c && $strn !~ /^[-+]?\d+$/);
    if ($n <= $poscmp && $n >= $negcmp) {
      $_[0] = $strn if ref($n);
    } else {
      $_[0] = Math::BigInt->new($strn)
    }
  }
  $_[0]->upgrade(undef) if ref($_[0]) && $_[0]->upgrade();
  1;
}

sub _binary_search {
  my($n, $lo, $hi, $sub, $exitsub) = @_;
  while ($lo < $hi) {
    my $mid = $lo + int(($hi-$lo) >> 1);
    return $mid if defined $exitsub && $exitsub->($n,$lo,$hi);
    if ($sub->($mid) < $n) { $lo = $mid+1; }
    else                   { $hi = $mid;   }
  }
  return $lo-1;
}

my @_primes_small = (0,2);
{
  my($n, $s, $sieveref) = (7-2, 3, _sieve_erat_string(5003));
  push @_primes_small, 2*pos($$sieveref)-1 while $$sieveref =~ m/0/g;
}
my @_prime_next_small = (
   2,2,3,5,5,7,7,11,11,11,11,13,13,17,17,17,17,19,19,23,23,23,23,
   29,29,29,29,29,29,31,31,37,37,37,37,37,37,41,41,41,41,43,43,47,
   47,47,47,53,53,53,53,53,53,59,59,59,59,59,59,61,61,67,67,67,67,67,67,71);

# For wheel-30
my @_prime_indices = (1, 7, 11, 13, 17, 19, 23, 29);
my @_nextwheel30 = (1,7,7,7,7,7,7,11,11,11,11,13,13,17,17,17,17,19,19,23,23,23,23,29,29,29,29,29,29,1);
my @_prevwheel30 = (29,29,1,1,1,1,1,1,7,7,7,7,11,11,13,13,13,13,17,17,19,19,19,19,23,23,23,23,23,23);
my @_wheeladvance30 = (1,6,5,4,3,2,1,4,3,2,1,2,1,4,3,2,1,2,1,4,3,2,1,6,5,4,3,2,1,2);
my @_wheelretreat30 = (1,2,1,2,3,4,5,6,1,2,3,4,1,2,1,2,3,4,1,2,1,2,3,4,1,2,3,4,5,6);

sub _tiny_prime_count {
  my($n) = @_;
  return if $n >= $_primes_small[-1];
  my $j = $#_primes_small;
  my $i = 1 + ($n >> 4);
  while ($i < $j) {
    my $mid = ($i+$j)>>1;
    if ($_primes_small[$mid] <= $n) { $i = $mid+1; }
    else                            { $j = $mid;   }
  }
  return $i-1;
}

sub _is_prime7 {  # n must not be divisible by 2, 3, or 5
  my($n) = @_;

  $n = _bigint_to_int($n) if ref($n) eq 'Math::BigInt' && $n <= BMAX;
  if (ref($n) eq 'Math::BigInt') {
    return 0 unless Math::BigInt::bgcd($n, B_PRIM767)->is_one;
    return 0 unless _miller_rabin_2($n);
    my $is_esl_prime = is_extra_strong_lucas_pseudoprime($n);
    return ($is_esl_prime)  ?  (($n <= "18446744073709551615") ? 2 : 1)  :  0;
  }

  if ($n < 61*61) {
    foreach my $i (qw/7 11 13 17 19 23 29 31 37 41 43 47 53 59/) {
      return 2 if $i*$i > $n;
      return 0 if !($n % $i);
    }
    return 2;
  }

  return 0 if !($n %  7) || !($n % 11) || !($n % 13) || !($n % 17) ||
              !($n % 19) || !($n % 23) || !($n % 29) || !($n % 31) ||
              !($n % 37) || !($n % 41) || !($n % 43) || !($n % 47) ||
              !($n % 53) || !($n % 59);

  # We could do:
  #   return is_strong_pseudoprime($n, (2,299417)) if $n < 19471033;
  # or:
  #   foreach my $p (@_primes_small[18..168]) {
  #     last if $p > $limit;
  #     return 0 unless $n % $p;
  #   }
  #   return 2;

  if ($n <= 1_500_000) {
    my $limit = int(sqrt($n));
    my $i = 61;
    while (($i+30) <= $limit) {
      return 0 unless ($n% $i    ) && ($n%($i+ 6)) &&
                      ($n%($i+10)) && ($n%($i+12)) &&
                      ($n%($i+16)) && ($n%($i+18)) &&
                      ($n%($i+22)) && ($n%($i+28));
      $i += 30;
    }
    for my $inc (6,4,2,4,2,4,6,2) {
      last if $i > $limit;
      return 0 if !($n % $i);
      $i += $inc;
    }
    return 2;
  }

  if ($n < 47636622961201) {   # BPSW seems to be faster after this
    # Deterministic set of Miller-Rabin tests.  If the MR routines can handle
    # bases greater than n, then this can be simplified.
    my @bases;
    # n > 1_000_000 because of the previous block.
    if    ($n <         19471033) { @bases = ( 2,  299417); }
    elsif ($n <         38010307) { @bases = ( 2,  9332593); }
    elsif ($n <        316349281) { @bases = ( 11000544, 31481107); }
    elsif ($n <       4759123141) { @bases = ( 2, 7, 61); }
    elsif ($n <     154639673381) { @bases = ( 15, 176006322, 4221622697); }
    elsif ($n <   47636622961201) { @bases = ( 2, 2570940, 211991001, 3749873356); }
    elsif ($n < 3770579582154547) { @bases = ( 2, 2570940, 880937, 610386380, 4130785767); }
    else                          { @bases = ( 2, 325, 9375, 28178, 450775, 9780504, 1795265022); }
    return is_strong_pseudoprime($n, @bases)  ?  2  :  0;
  }

  # Inlined BPSW
  return 0 unless _miller_rabin_2($n);
  return is_almost_extra_strong_lucas_pseudoprime($n) ? 2 : 0;
}

sub is_prime {
  my($n) = @_;
  return 0 if defined($n) && int($n) < 0;
  _validate_positive_integer($n);

  if (ref($n) eq 'Math::BigInt') {
    return 0 unless Math::BigInt::bgcd($n, B_PRIM235)->is_one;
  } else {
    if ($n < 7) { return ($n == 2) || ($n == 3) || ($n == 5) ? 2 : 0; }
    return 0 if !($n % 2) || !($n % 3) || !($n % 5);
  }
  return _is_prime7($n);
}

# is_prob_prime is the same thing for us.
*is_prob_prime = \&is_prime;

# BPSW probable prime.  No composites are known to have passed this test
# since it was published in 1980, though we know infinitely many exist.
# It has also been verified that no 64-bit composite will return true.
# Slow since it's all in PP and uses bigints.
sub is_bpsw_prime {
  my($n) = @_;
  return 0 if defined($n) && int($n) < 0;
  _validate_positive_integer($n);
  return 0 unless _miller_rabin_2($n);
  if ($n <= 18446744073709551615) {
    return is_almost_extra_strong_lucas_pseudoprime($n) ? 2 : 0;
  }
  return is_extra_strong_lucas_pseudoprime($n) ? 1 : 0;
}

sub is_provable_prime {
  my($n) = @_;
  return 0 if defined $n && $n < 2;
  _validate_positive_integer($n);
  if ($n <= 18446744073709551615) {
    return 0 unless _miller_rabin_2($n);
    return 0 unless is_almost_extra_strong_lucas_pseudoprime($n);
    return 2;
  }
  my($is_prime, $cert) = Math::Prime::Util::is_provable_prime_with_cert($n);
  $is_prime;
}

# Possible sieve storage:
#   1) vec with mod-30 wheel:   8 bits  / 30
#   2) vec with mod-2 wheel :  15 bits  / 30
#   3) str with mod-30 wheel:   8 bytes / 30
#   4) str with mod-2 wheel :  15 bytes / 30
#
# It looks like using vecs is about 2x slower than strs, and the strings also
# let us do some fast operations on the results.  E.g.
#   Count all primes:
#      $count += $$sieveref =~ tr/0//;
#   Loop over primes:
#      foreach my $s (split("0", $$sieveref, -1)) {
#        $n += 2 + 2 * length($s);
#        .. do something with the prime $n
#      }
#
# We're using method 4, though sadly it is memory intensive relative to the
# other methods.  I will point out that it is 30-60x less memory than sieves
# using an array, and the performance of this function is over 10x that
# of naive sieves.

sub _sieve_erat_string {
  my($end) = @_;
  $end-- if ($end & 1) == 0;
  my $s_end = $end >> 1;

  my $whole = int( $s_end / 15);   # Prefill with 3 and 5 already marked.
  croak "Sieve too large" if $whole > 1_145_324_612;  # ~32 GB string
  my $sieve = '100010010010110' . '011010010010110' x $whole;
  substr($sieve, $s_end+1) = '';   # Ensure we don't make too many entries
  my ($n, $limit) = ( 7, int(sqrt($end)) );
  while ( $n <= $limit ) {
    for (my $s = ($n*$n) >> 1; $s <= $s_end; $s += $n) {
      substr($sieve, $s, 1) = '1';
    }
    do { $n += 2 } while substr($sieve, $n>>1, 1);
  }
  return \$sieve;
}

# TODO: this should be plugged into precalc, memfree, etc. just like the C code
{
  my $primary_size_limit = 15000;
  my $primary_sieve_size = 0;
  my $primary_sieve_ref;
  sub _sieve_erat {
    my($end) = @_;

    return _sieve_erat_string($end) if $end > $primary_size_limit;

    if ($primary_sieve_size == 0) {
      $primary_sieve_size = $primary_size_limit;
      $primary_sieve_ref = _sieve_erat_string($primary_sieve_size);
    }
    my $sieve = substr($$primary_sieve_ref, 0, ($end+1)>>1);
    return \$sieve;
  }
}


sub _sieve_segment {
  my($beg,$end,$limit) = @_;
  ($beg, $end) = map { _bigint_to_int($_) } ($beg, $end)
    if ref($end) && $end <= BMAX;
  croak "Internal error: segment beg is even" if ($beg % 2) == 0;
  croak "Internal error: segment end is even" if ($end % 2) == 0;
  croak "Internal error: segment end < beg" if $end < $beg;
  croak "Internal error: segment beg should be >= 3" if $beg < 3;
  my $range = int( ($end - $beg) / 2 ) + 1;

  # Prefill with 3 and 5 already marked, and offset to the segment start.
  my $whole = int( ($range+14) / 15);
  my $startp = ($beg % 30) >> 1;
  my $sieve = substr('011010010010110', $startp) . '011010010010110' x $whole;
  # Set 3 and 5 to prime if we're sieving them.
  substr($sieve,0,2) = '00' if $beg == 3;
  substr($sieve,0,1) = '0'  if $beg == 5;
  # Get rid of any extra we added.
  substr($sieve, $range) = '';

  # If the end value is below 7^2, then the pre-sieve is all we needed.
  return \$sieve if $end < 49;

  my $sqlimit = ref($end) ? $end->copy->bsqrt() : int(sqrt($end)+0.0000001);
  $limit = $sqlimit if !defined $limit || $sqlimit < $limit;
  # For large value of end, it's a huge win to just walk primes.

  my($p, $s, $primesieveref) = (7-2, 3, _sieve_erat($limit));
  while ( (my $nexts = 1 + index($$primesieveref, '0', $s)) > 0 ) {
    $p += 2 * ($nexts - $s);
    $s = $nexts;
    my $p2 = $p*$p;
    if ($p2 < $beg) {
      my $f = 1+int(($beg-1)/$p);
      $f++ unless $f % 2;
      $p2 = $p * $f;
    }
    # With large bases and small segments, it's common to find we don't hit
    # the segment at all.  Skip all the setup if we find this now.
    if ($p2 <= $end) {
      # Inner loop marking multiples of p
      # (everything is divided by 2 to keep inner loop simpler)
      my $filter_end = ($end - $beg) >> 1;
      my $filter_p2  = ($p2  - $beg) >> 1;
      while ($filter_p2 <= $filter_end) {
        substr($sieve, $filter_p2, 1) = "1";
        $filter_p2 += $p;
      }
    }
  }
  \$sieve;
}

sub trial_primes {
  my($low,$high) = @_;
  if (!defined $high) {
    $high = $low;
    $low = 2;
  }
  _validate_positive_integer($low);
  _validate_positive_integer($high);
  return if $low > $high;
  my @primes;

  # For a tiny range, just use next_prime calls
  if (($high-$low) < 1000) {
    $low-- if $low >= 2;
    my $curprime = next_prime($low);
    while ($curprime <= $high) {
      push @primes, $curprime;
      $curprime = next_prime($curprime);
    }
    return \@primes;
  }

  # Sieve to 10k then BPSW test
  push @primes, 2  if ($low <= 2) && ($high >= 2);
  push @primes, 3  if ($low <= 3) && ($high >= 3);
  push @primes, 5  if ($low <= 5) && ($high >= 5);
  $low = 7 if $low < 7;
  $low++ if ($low % 2) == 0;
  $high-- if ($high % 2) == 0;
  my $sieveref = _sieve_segment($low, $high, 10000);
  my $n = $low-2;
  while ($$sieveref =~ m/0/g) {
    my $p = $n+2*pos($$sieveref);
    push @primes, $p if _miller_rabin_2($p) && is_extra_strong_lucas_pseudoprime($p);
  }
  return \@primes;
}

sub primes {
  my($low,$high) = @_;
  if (scalar @_ > 1) {
    _validate_positive_integer($low);
    _validate_positive_integer($high);
    $low = 2 if $low < 2;
  } else {
    ($low,$high) = (2, $low);
    _validate_positive_integer($high);
  }
  my $sref = [];
  return $sref if ($low > $high) || ($high < 2);
  return [grep { $_ >= $low && $_ <= $high } @_primes_small]
    if $high <= $_primes_small[-1];

  return [ Math::Prime::Util::GMP::sieve_primes($low, $high, 0) ]
    if $Math::Prime::Util::_GMPfunc{"sieve_primes"} && $Math::Prime::Util::GMP::VERSION >= 0.34;

  # At some point even the pretty-fast pure perl sieve is going to be a
  # dog, and we should move to trials.  This is typical with a small range
  # on a large base.  More thought on the switchover should be done.
  return trial_primes($low, $high) if ref($low)  eq 'Math::BigInt'
                                   || ref($high) eq 'Math::BigInt'
                                   || ($low > 1_000_000_000_000 && ($high-$low) < int($low/1_000_000));

  push @$sref, 2  if ($low <= 2) && ($high >= 2);
  push @$sref, 3  if ($low <= 3) && ($high >= 3);
  push @$sref, 5  if ($low <= 5) && ($high >= 5);
  $low = 7 if $low < 7;
  $low++ if ($low % 2) == 0;
  $high-- if ($high % 2) == 0;
  return $sref if $low > $high;

  my($n,$sieveref);
  if ($low == 7) {
    $n = 0;
    $sieveref = _sieve_erat($high);
    substr($$sieveref,0,3,'111');
  } else {
    $n = $low-1;
    $sieveref = _sieve_segment($low,$high);
  }
  push @$sref, $n+2*pos($$sieveref)-1 while $$sieveref =~ m/0/g;
  $sref;
}

sub sieve_range {
  my($n, $width, $depth) = @_;
  _validate_positive_integer($n);
  _validate_positive_integer($width);
  _validate_positive_integer($depth);

  my @candidates;
  my $start = $n;

  if ($n < 5) {
    push @candidates, (2-$n) if $n <= 2 && $n+$width-1 >= 2;
    push @candidates, (3-$n) if $n <= 3 && $n+$width-1 >= 3;
    push @candidates, (4-$n) if $n <= 4 && $n+$width-1 >= 4 && $depth < 2;
    $start = 5;
    $width -= ($start - $n);
  }

  return @candidates, map {$start+$_-$n } 0 .. $width-1 if $depth < 2;
  return @candidates, map { $_ - $n }
                      grep { ($_ & 1) && ($depth < 3 || ($_ % 3)) }
                      map { $start+$_ }
                      0 .. $width-1                     if $depth < 5;

  if (!($start & 1)) { $start++; $width--; }
  $width-- if !($width&1);
  return @candidates if $width < 1;

  my $sieveref = _sieve_segment($start, $start+$width-1, $depth);
  my $offset = $start - $n - 2;
  while ($$sieveref =~ m/0/g) {
    push @candidates, $offset + (pos($$sieveref) << 1);
  }
  return @candidates;
}

sub sieve_prime_cluster {
  my($lo,$hi,@cl) = @_;
  my $_verbose = Math::Prime::Util::prime_get_config()->{'verbose'};
  _validate_positive_integer($lo);
  _validate_positive_integer($hi);

  if ($Math::Prime::Util::_GMPfunc{"sieve_prime_cluster"}) {
    return map { ($_ > ''.~0) ? Math::BigInt->new(''.$_) : $_ }
           Math::Prime::Util::GMP::sieve_prime_cluster($lo,$hi,@cl);
  }

  return @{primes($lo,$hi)} if scalar(@cl) == 0;

  unshift @cl, 0;
  for my $i (1 .. $#cl) {
    _validate_positive_integer($cl[$i]);
    croak "sieve_prime_cluster: values must be even" if $cl[$i] & 1;
    croak "sieve_prime_cluster: values must be increasing" if $cl[$i] <= $cl[$i-1];
  }
  my($p,$sievelim,@p) = (17, 2000);
  $p = 13 if ($hi-$lo) < 50_000_000;
  $p = 11 if ($hi-$lo) <  1_000_000;
  $p =  7 if ($hi-$lo) <     20_000 && $lo < INTMAX;

  # Add any cases under our sieving point.
  if ($lo <= $sievelim) {
    $sievelim = $hi if $sievelim > $hi;
    for my $n (@{primes($lo,$sievelim)}) {
      my $ac = 1;
      for my $ci (1 .. $#cl) {
        if (!is_prime($n+$cl[$ci])) { $ac = 0; last; }
      }
      push @p, $n if $ac;
    }
    $lo = next_prime($sievelim);
  }
  return @p if $lo > $hi;

  # Compute acceptable residues.
  my $pr = primorial($p);
  my $startpr = _bigint_to_int($lo % $pr);

  my @acc = grep { ($_ & 1) && $_%3 }  ($startpr .. $startpr + $pr - 1);
  for my $c (@cl) {
    if ($p >= 7) {
      @acc = grep { (($_+$c)%3) && (($_+$c)%5) && (($_+$c)%7) } @acc;
    } else {
      @acc = grep { (($_+$c)%3)  && (($_+$c)%5) } @acc;
    }
  }
  for my $c (@cl) {
    @acc = grep { Math::Prime::Util::gcd($_+$c,$pr) == 1 } @acc;
  }
  @acc = map { $_-$startpr } @acc;

  print "cluster sieve using ",scalar(@acc)," residues mod $pr\n" if $_verbose;
  return @p if scalar(@acc) == 0;

  # Prepare table for more sieving.
  my @mprimes = @{primes( $p+1, $sievelim)};
  my @vprem;
  for my $p (@mprimes) {
    for my $c (@cl) {
      $vprem[$p]->[ ($p-($c%$p)) % $p ] = 1;
    }
  }

  # Walk the range in primorial chunks, doing primality tests.
  my($nummr, $numlucas) = (0,0);
  while ($lo <= $hi) {

    my @racc = @acc;

    # Make sure we don't do anything past the limit
    if (($lo+$acc[-1]) > $hi) {
      my $max = _bigint_to_int($hi-$lo);
      @racc = grep { $_ <= $max } @racc;
    }

    # Sieve more values using native math
    foreach my $p (@mprimes) {
      my $rem = _bigint_to_int( $lo % $p );
      @racc = grep { !$vprem[$p]->[ ($rem+$_) % $p ] } @racc;
      last unless scalar(@racc);
    }

    # Do final primality tests.
    if ($lo < 1e13) {
      for my $r (@racc) {
        my($good, $p) = (1, $lo + $r);
        for my $c (@cl) {
          $nummr++;
          if (!Math::Prime::Util::is_prime($p+$c)) { $good = 0; last; }
        }
        push @p, $p if $good;
      }
    } else {
      for my $r (@racc) {
        my($good, $p) = (1, $lo + $r);
        for my $c (@cl) {
          $nummr++;
          if (!Math::Prime::Util::is_strong_pseudoprime($p+$c,2)) { $good = 0; last; }
        }
        next unless $good;
        for my $c (@cl) {
          $numlucas++;
          if (!Math::Prime::Util::is_extra_strong_lucas_pseudoprime($p+$c)) { $good = 0; last; }
        }
        push @p, $p if $good;
      }
    }

    $lo += $pr;
  }
  print "cluster sieve ran $nummr MR and $numlucas Lucas tests\n" if $_verbose;
  @p;
}


sub _n_ramanujan_primes {
  my($n) = @_;
  return [] if $n <= 0;
  my $max = nth_prime_upper(int(48/19*$n)+1);
  my @L = (2, (0) x $n-1);
  my $s = 1;
  for (my $k = 7; $k <= $max; $k += 2) {
    $s++ if is_prime($k);
    $L[$s] = $k+1 if $s < $n;
    $s-- if ($k&3) == 1 && is_prime(($k+1)>>1);
    $L[$s] = $k+2 if $s < $n;
  }
  \@L;
}

sub _ramanujan_primes {
  my($low,$high) = @_;
  ($low,$high) = (2, $low) unless defined $high;
  return [] if ($low > $high) || ($high < 2);
  my $nn = prime_count_upper($high) >> 1;
  my $L = _n_ramanujan_primes($nn);
  shift @$L while @$L && $L->[0] < $low;
  pop @$L while @$L && $L->[-1] > $high;
  $L;
}

sub is_ramanujan_prime {
  my($n) = @_;
  return 1 if $n == 2;
  return 0 if $n < 11;
  my $L = _ramanujan_primes($n,$n);
  return (scalar(@$L) > 0) ? 1 : 0;
}

sub nth_ramanujan_prime {
  my($n) = @_;
  return undef if $n <= 0;  ## no critic qw(ProhibitExplicitReturnUndef)
  my $L = _n_ramanujan_primes($n);
  return $L->[$n-1];
}

sub next_prime {
  my($n) = @_;
  _validate_positive_integer($n);
  return $_prime_next_small[$n] if $n <= $#_prime_next_small;
  # This turns out not to be faster.
  # return $_primes_small[1+_tiny_prime_count($n)] if $n < $_primes_small[-1];

  return Math::BigInt->new(MPU_32BIT ? "4294967311" : "18446744073709551629")
    if ref($n) ne 'Math::BigInt' && $n >= MPU_MAXPRIME;
  # n is now either 1) not bigint and < maxprime, or (2) bigint and >= uvmax

  if ($n > 4294967295 && Math::Prime::Util::prime_get_config()->{'gmp'}) {
    return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::next_prime($n));
  }

  if (ref($n) eq 'Math::BigInt') {
    do {
      $n += $_wheeladvance30[$n%30];
    } while !Math::BigInt::bgcd($n, B_PRIM767)->is_one ||
            !_miller_rabin_2($n) || !is_extra_strong_lucas_pseudoprime($n);
  } else {
    do {
      $n += $_wheeladvance30[$n%30];
    } while !($n%7) || !_is_prime7($n);
  }
  $n;
}

sub prev_prime {
  my($n) = @_;
  _validate_positive_integer($n);
  return (undef,undef,undef,2,3,3,5,5,7,7,7,7)[$n] if $n <= 11;
  if ($n > 4294967295 && Math::Prime::Util::prime_get_config()->{'gmp'}) {
    return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::prev_prime($n));
  }

  if (ref($n) eq 'Math::BigInt') {
    do {
      $n -= $_wheelretreat30[$n%30];
    } while !Math::BigInt::bgcd($n, B_PRIM767)->is_one ||
            !_miller_rabin_2($n) || !is_extra_strong_lucas_pseudoprime($n);
  } else {
    do {
      $n -= $_wheelretreat30[$n%30];
    } while !($n%7) || !_is_prime7($n);
  }
  $n;
}

sub partitions {
  my $n = shift;

  my $d = int(sqrt($n+1));
  my @pent = (1, map { (($_*(3*$_+1))>>1, (($_+1)*(3*$_+2))>>1) } 1 .. $d);
  my $ZERO = ($n >= ((~0 > 4294967295) ? 400 : 270)) ? BZERO : 0;
  my @part = ($ZERO+1);
  foreach my $j (scalar @part .. $n) {
    my ($psum1, $psum2, $k) = ($ZERO, $ZERO, 1);
    foreach my $p (@pent) {
      last if $p > $j;
      if ((++$k) & 2) { $psum1 += $part[ $j - $p ] }
      else            { $psum2 += $part[ $j - $p ] }
    }
    $part[$j] = $psum1 - $psum2;
  }
  return $part[$n];
}

sub primorial {
  my $n = shift;

  my @plist = @{primes($n)};
  my $max = (MPU_32BIT) ? 29 : (OLD_PERL_VERSION) ? 43 : 53;

  # If small enough, multiply the small primes.
  if ($n < $max) {
    my $pn = 1;
    $pn *= $_ for @plist;
    return $pn;
  }

  # Otherwise, combine them as UVs, then combine using product tree.
  my $i = 0;
  while ($i < $#plist) {
    my $m = $plist[$i] * $plist[$i+1];
    if ($m <= INTMAX) { splice(@plist, $i, 2, $m); }
    else              { $i++;                      }
  }
  vecprod(@plist);
}

sub consecutive_integer_lcm {
  my $n = shift;

  my $max = (MPU_32BIT) ? 22 : (OLD_PERL_VERSION) ? 37 : 46;
  my $pn = ref($n) ? ref($n)->new(1) : ($n >= $max) ? Math::BigInt->bone() : 1;
  for (my $p = 2; $p <= $n; $p = next_prime($p)) {
    my($p_power, $pmin) = ($p, int($n/$p));
    $p_power *= $p while $p_power <= $pmin;
    $pn *= $p_power;
  }
  $pn = _bigint_to_int($pn) if $pn <= BMAX;
  return $pn;
}

sub jordan_totient {
  my($k, $n) = @_;
  return ($n == 1) ? 1 : 0  if $k == 0;
  return euler_phi($n)      if $k == 1;
  return ($n == 1) ? 1 : 0  if $n <= 1;

  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::jordan_totient($k, $n))
    if $Math::Prime::Util::_GMPfunc{"jordan_totient"};


  my @pe = Math::Prime::Util::factor_exp($n);
  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
  my $totient = BONE->copy;
  foreach my $f (@pe) {
    my ($p, $e) = @$f;
    $p = Math::BigInt->new("$p")->bpow($k);
    $totient->bmul($p->copy->bdec());
    $totient->bmul($p) for 2 .. $e;
  }
  $totient = _bigint_to_int($totient) if $totient->bacmp(BMAX) <= 0;
  return $totient;
}

sub euler_phi {
  return euler_phi_range(@_) if scalar @_ > 1;
  my($n) = @_;
  return 0 if defined $n && $n < 0;

  return Math::Prime::Util::_reftyped($_[0],Math::Prime::Util::GMP::totient($n))
    if $Math::Prime::Util::_GMPfunc{"totient"};

  _validate_positive_integer($n);
  return $n if $n <= 1;

  my $totient = $n - $n + 1;

  # Fast reduction of multiples of 2, may also reduce n for factoring
  if (ref($n) eq 'Math::BigInt') {
    my $s = 0;
    if ($n->is_even) {
      do { $n->brsft(BONE); $s++; } while $n->is_even;
      $totient->blsft($s-1) if $s > 1;
    }
  } else {
    while (($n % 4) == 0) { $n >>= 1;  $totient <<= 1; }
    if (($n % 2) == 0) { $n >>= 1; }
  }

  my @pe = Math::Prime::Util::factor_exp($n);

  if ($#pe == 0 && $pe[0]->[1] == 1) {
    if (ref($n) ne 'Math::BigInt') { $totient *= $n-1; }
    else                           { $totient->bmul($n->bdec()); }
  } elsif (ref($n) ne 'Math::BigInt') {
    foreach my $f (@pe) {
      my ($p, $e) = @$f;
      $totient *= $p - 1;
      $totient *= $p for 2 .. $e;
    }
  } else {
    my $zero = $n->copy->bzero;
    foreach my $f (@pe) {
      my ($p, $e) = @$f;
      $p = $zero->copy->badd("$p");
      $totient->bmul($p->copy->bdec());
      $totient->bmul($p) for 2 .. $e;
    }
  }
  $totient = _bigint_to_int($totient) if ref($totient) eq 'Math::BigInt'
                                      && $totient->bacmp(BMAX) <= 0;
  return $totient;
}

sub inverse_totient {
  my($n) = @_;
  _validate_positive_integer($n);

  return wantarray ? (1,2) : 2 if $n == 1;
  return wantarray ? () : 0 if $n < 1 || ($n & 1);

  $n = Math::Prime::Util::_to_bigint("$n") if !ref($n) && $n > 2**49;
  my $do_bigint = ref($n);

  if (is_prime($n >> 1)) {   # Coleman Remark 3.3 (Thm 3.1) and Prop 6.2
    return wantarray ? () : 0             if !is_prime($n+1);
    return wantarray ? ($n+1, 2*$n+2) : 2 if $n >= 10;
  }

  if (!wantarray) {
    my %r = ( 1 => 1 );
    Math::Prime::Util::fordivisors(sub { my $d = $_;
      $d = $do_bigint->new("$d") if $do_bigint;
      my $p = $d+1;
      if (Math::Prime::Util::is_prime($p)) {
        my($dp,@sumi,@sumv) = ($d);
        for my $v (1 .. 1 + Math::Prime::Util::valuation($n, $p)) {
          Math::Prime::Util::fordivisors(sub { my $d2 = $_;
            if (defined $r{$d2}) { push @sumi, $d2*$dp; push @sumv, $r{$d2}; }
          }, $n / $dp);
          $dp *= $p;
        }
        $r{ $sumi[$_] } += $sumv[$_]  for 0 .. $#sumi;
      }
    }, $n);
    return (defined $r{$n}) ? $r{$n} : 0;
  } else {
    my %r = ( 1 => [1] );
    Math::Prime::Util::fordivisors(sub { my $d = $_;
      $d = $do_bigint->new("$d") if $do_bigint;
      my $p = $d+1;
      if (Math::Prime::Util::is_prime($p)) {
        my($dp,$pp,@T) = ($d,$p);
        for my $v (1 .. 1 + Math::Prime::Util::valuation($n, $p)) {
          Math::Prime::Util::fordivisors(sub { my $d2 = $_;
            push @T, [ $d2*$dp, [map { $_ * $pp } @{ $r{$d2} }] ] if defined $r{$d2};
          }, $n / $dp);
          $dp *= $p;
          $pp *= $p;
        }
        push @{$r{$_->[0]}}, @{$_->[1]} for @T;
      }
    }, $n);
    return () unless defined $r{$n};
    delete @r{ grep { $_ != $n } keys %r };  # Delete all intermediate results
    my @result = sort { $a <=> $b } @{$r{$n}};
    return @result;
  }
}

sub euler_phi_range {
  my($lo, $hi) = @_;
  _validate_integer($lo);
  _validate_integer($hi);

  my @totients;
  while ($lo < 0 && $lo <= $hi) {
    push @totients, 0;
    $lo++;
  }
  return @totients if $hi < $lo;

  if ($hi > 2**30 || $hi-$lo < 100) {
    while ($lo <= $hi) {
      push @totients, euler_phi($lo++);
    }
  } else {
    my @tot = (0 .. $hi);
    foreach my $i (2 .. $hi) {
      next unless $tot[$i] == $i;
      $tot[$i] = $i-1;
      foreach my $j (2 .. int($hi / $i)) {
        $tot[$i*$j] -= $tot[$i*$j]/$i;
      }
    }
    splice(@tot, 0, $lo) if $lo > 0;
    push @totients, @tot;
  }
  @totients;
}

sub moebius {
  return moebius_range(@_) if scalar @_ > 1;
  my($n) = @_;
  $n = -$n if defined $n && $n < 0;
  _validate_num($n) || _validate_positive_integer($n);
  return ($n == 1) ? 1 : 0  if $n <= 1;
  return 0 if ($n >= 49) && (!($n % 4) || !($n % 9) || !($n % 25) || !($n%49) );
  my @factors = Math::Prime::Util::factor($n);
  foreach my $i (1 .. $#factors) {
    return 0 if $factors[$i] == $factors[$i-1];
  }
  return ((scalar @factors) % 2) ? -1 : 1;
}
sub is_square_free {
  return (Math::Prime::Util::moebius($_[0]) != 0) ? 1 : 0;
}
sub is_semiprime {
  my($n) = @_;
  _validate_positive_integer($n);
  return ($n == 4) if $n < 6;
  return (Math::Prime::Util::is_prob_prime($n>>1) ? 1 : 0) if ($n % 2) == 0;
  return (Math::Prime::Util::is_prob_prime($n/3)  ? 1 : 0) if ($n % 3) == 0;
  return (Math::Prime::Util::is_prob_prime($n/5)  ? 1 : 0) if ($n % 5) == 0;
  {
    my @f = trial_factor($n, 4999);
    return 0 if @f > 2;
    return (_is_prime7($f[1]) ? 1 : 0) if @f == 2;
  }
  return 0 if _is_prime7($n);
  {
    my @f = pminus1_factor ($n, 250_000);
    return 0 if @f > 2;
    return (_is_prime7($f[1]) ? 1 : 0) if @f == 2;
  }
  {
    my @f = pbrent_factor ($n, 128*1024, 3, 1);
    return 0 if @f > 2;
    return (_is_prime7($f[1]) ? 1 : 0) if @f == 2;
  }
  return (scalar(Math::Prime::Util::factor($n)) == 2) ? 1 : 0;
}

sub _totpred {
  my($n, $maxd) = @_;
  return 0 if $maxd <= 1 || (ref($n) ? $n->is_odd() : ($n & 1));
  $n = Math::BigInt->new("$n") unless ref($n) || $n < INTMAX;
  $n >>= 1;
  return 1 if $n == 1 || ($n < $maxd && Math::Prime::Util::is_prime(2*$n+1));
  for my $d (Math::Prime::Util::divisors($n)) {
    last if $d >= $maxd;
    my $p = ($d < (INTMAX >> 1))  ?  ($d<<1)+1  :  Math::Prime::Util::vecprod(2,$d)+1;
    next unless Math::Prime::Util::is_prime($p);
    my $r = int($n / $d);
    while (1) {
      return 1 if $r == $p || _totpred($r, $d);
      last if $r % $p;
      $r = int($r / $p);
    }
  }
  0;
}
sub is_totient {
  my($n) = @_;
  _validate_positive_integer($n);
  return 1 if $n == 1;
  return 0 if $n <= 0;
  return _totpred($n,$n);
}


sub moebius_range {
  my($lo, $hi) = @_;
  _validate_integer($lo);
  _validate_integer($hi);
  return () if $hi < $lo;
  return moebius($lo) if $lo == $hi;
  if ($lo < 0) {
    if ($hi < 0) {
      return reverse(moebius_range(-$hi,-$lo));
    } else {
      return (reverse(moebius_range(1,-$lo)), moebius_range(0,$hi));
    }
  }
  if ($hi > 2**32) {
    my @mu;
    while ($lo <= $hi) {
      push @mu, moebius($lo++);
    }
    return @mu;
  }
  my @mu = map { 1 } $lo .. $hi;
  $mu[0] = 0 if $lo == 0;
  my($p, $sqrtn) = (2, int(sqrt($hi)+0.5));
  while ($p <= $sqrtn) {
    my $i = $p * $p;
    $i = $i * int($lo/$i) + (($lo % $i)  ? $i : 0)  if $i < $lo;
    while ($i <= $hi) {
      $mu[$i-$lo] = 0;
      $i += $p * $p;
    }
    $i = $p;
    $i = $i * int($lo/$i) + (($lo % $i)  ? $i : 0)  if $i < $lo;
    while ($i <= $hi) {
      $mu[$i-$lo] *= -$p;
      $i += $p;
    }
    $p = next_prime($p);
  }
  foreach my $i ($lo .. $hi) {
    my $m = $mu[$i-$lo];
    $m *= -1 if abs($m) != $i;
    $mu[$i-$lo] = ($m>0) - ($m<0);
  }
  return @mu;
}

sub mertens {
  my($n) = @_;
    # This is the most basic Deléglise and Rivat algorithm.  u = n^1/2
  # and no segmenting is done.  Their algorithm uses u = n^1/3, breaks
  # the summation into two parts, and calculates those in segments.  Their
  # computation time growth is half of this code.
  return $n if $n <= 1;
  my $u = int(sqrt($n));
  my @mu = (0, Math::Prime::Util::moebius(1, $u)); # Hold values of mu for 0-u
  my $musum = 0;
  my @M = map { $musum += $_; } @mu;     # Hold values of M for 0-u
  my $sum = $M[$u];
  foreach my $m (1 .. $u) {
    next if $mu[$m] == 0;
    my $inner_sum = 0;
    my $lower = int($u/$m) + 1;
    my $last_nmk = int($n/($m*$lower));
    my ($denom, $this_k, $next_k) = ($m, 0, int($n/($m*1)));
    for my $nmk (1 .. $last_nmk) {
      $denom += $m;
      $this_k = int($n/$denom);
      next if $this_k == $next_k;
      ($this_k, $next_k) = ($next_k, $this_k);
      $inner_sum += $M[$nmk] * ($this_k - $next_k);
    }
    $sum -= $mu[$m] * $inner_sum;
  }
  return $sum;
}

sub ramanujan_sum {
  my($k,$n) = @_;
  return 0 if $k < 1 || $n <  1;
  my $g = $k / Math::Prime::Util::gcd($k,$n);
  my $m = Math::Prime::Util::moebius($g);
  return $m if $m == 0 || $k == $g;
  $m * (Math::Prime::Util::euler_phi($k) / Math::Prime::Util::euler_phi($g));
}

sub liouville {
  my($n) = @_;
  my $l = (-1) ** scalar Math::Prime::Util::factor($n);
  return $l;
}

# Exponential of Mangoldt function (A014963).
# Return p if n = p^m [p prime, m >= 1], 1 otherwise.
sub exp_mangoldt {
  my($n) = @_;
  my $p;
  return 1 unless Math::Prime::Util::is_prime_power($n,\$p);
  $p;
}

sub carmichael_lambda {
  my($n) = @_;
  return euler_phi($n) if $n < 8;          # = phi(n) for n < 8
  return $n >> 2 if ($n & ($n-1)) == 0;    # = phi(n)/2 = n/4 for 2^k, k>2

  my @pe = Math::Prime::Util::factor_exp($n);
  $pe[0]->[1]-- if $pe[0]->[0] == 2 && $pe[0]->[1] > 2;

  my $lcm;
  if (!ref($n)) {
    $lcm = Math::Prime::Util::lcm(
      map { ($_->[0] ** ($_->[1]-1)) * ($_->[0]-1) } @pe
    );
  } else {
    $lcm = Math::BigInt::blcm(
      map { $_->[0]->copy->bpow($_->[1]->copy->bdec)->bmul($_->[0]->copy->bdec) }
      map { [ map { Math::BigInt->new("$_") } @$_ ] }
      @pe
    );
    $lcm = _bigint_to_int($lcm) if $lcm->bacmp(BMAX) <= 0;
  }
  $lcm;
}

sub is_carmichael {
  my($n) = @_;
  _validate_positive_integer($n);

  # This works fine, but very slow
  # return !is_prime($n) && ($n % carmichael_lambda($n)) == 1;

  return 0 if $n < 561 || ($n % 2) == 0;
  return 0 if (!($n % 9) || !($n % 25) || !($n%49) || !($n%121));

  # Check Korselt's criterion for small divisors
  my $fn = $n;
  for my $a (5,7,11,13,17,19,23,29,31,37,41,43) {
    if (($fn % $a) == 0) {
      return 0 if (($n-1) % ($a-1)) != 0;   # Korselt
      $fn /= $a;
      return 0 unless $fn % $a;             # not square free
    }
  }
  return 0 if Math::Prime::Util::powmod(2, $n-1, $n) != 1;

  # After pre-tests, it's reasonably likely $n is a Carmichael number or prime

  # Use probabilistic test if too large to reasonably factor.
  if (length($fn) > 50) {
    return 0 if Math::Prime::Util::is_prime($n);
    for my $t (13 .. 150) {
      my $a = $_primes_small[$t];
      my $gcd = Math::Prime::Util::gcd($a, $fn);
      if ($gcd == 1) {
        return 0 if Math::Prime::Util::powmod($a, $n-1, $n) != 1;
      } else {
        return 0 if $gcd != $a;              # Not square free
        return 0 if (($n-1) % ($a-1)) != 0;  # factor doesn't divide
        $fn /= $a;
      }
    }
    return 1;
  }

  # Verify with factoring.
  my @pe = Math::Prime::Util::factor_exp($n);
  return 0 if scalar(@pe) < 3;
  for my $pe (@pe) {
    return 0 if $pe->[1] > 1 || (($n-1) % ($pe->[0]-1)) != 0;
  }
  1;
}

sub is_quasi_carmichael {
  my($n) = @_;
  _validate_positive_integer($n);

  return 0 if $n < 35;
  return 0 if (!($n % 4) || !($n % 9) || !($n % 25) || !($n%49) || !($n%121));

  my @pe = Math::Prime::Util::factor_exp($n);
  # Not quasi-Carmichael if prime
  return 0 if scalar(@pe) < 2;
  # Not quasi-Carmichael if not square free
  for my $pe (@pe) {
    return 0 if $pe->[1] > 1;
  }
  my @f = map { $_->[0] } @pe;
  my $nbases = 0;
  if ($n < 2000) {
    # In theory for performance, but mainly keeping to show direct method.
    my $lim = $f[-1];
    $lim = (($n-$lim*$lim) + $lim - 1) / $lim;
    for my $b (1 .. $f[0]-1) {
      my $nb = $n - $b;
      $nbases++ if Math::Prime::Util::vecall(sub { $nb % ($_-$b) == 0 }, @f);
    }
    if (scalar(@f) > 2) {
      for my $b (1 .. $lim-1) {
        my $nb = $n + $b;
        $nbases++ if Math::Prime::Util::vecall(sub { $nb % ($_+$b) == 0 }, @f);
      }
    }
  } else {
    my($spf,$lpf) = ($f[0], $f[-1]);
    if (scalar(@f) == 2) {
      foreach my $d (Math::Prime::Util::divisors($n/$spf - 1)) {
        my $k = $spf - $d;
        my $p = $n - $k;
        last if $d >= $spf;
        $nbases++ if Math::Prime::Util::vecall(sub { my $j = $_-$k;  $j && ($p % $j) == 0 }, @f);
      }
    } else {
      foreach my $d (Math::Prime::Util::divisors($lpf * ($n/$lpf - 1))) {
        my $k = $lpf - $d;
        my $p = $n - $k;
        next if $k == 0 || $k >= $spf;
        $nbases++ if Math::Prime::Util::vecall(sub { my $j = $_-$k;  $j && ($p % $j) == 0 }, @f);
      }
    }
  }
  $nbases;
}

sub is_pillai {
  my($p) = @_;
  return 0 if defined($p) && int($p) < 0;
  _validate_positive_integer($p);
  return 0 if $p <= 2;

  my $pm1 = $p-1;
  my $nfac = 5040 % $p;
  for (my $n = 8; $n < $p; $n++) {
    $nfac = Math::Prime::Util::mulmod($nfac, $n, $p);
    return $n if $nfac == $pm1 && ($p % $n) != 1;
  }
  0;
}

sub is_fundamental {
  my($n) = @_;
  _validate_integer($n);
  my $neg = ($n < 0);
  $n = -$n if $neg;
  my $r = $n & 15;
  if ($r) {
    my $r4 = $r & 3;
    if (!$neg) {
      return (($r ==  4) ? 0 : is_square_free($n >> 2)) if $r4 == 0;
      return is_square_free($n) if $r4 == 1;
    } else {
      return (($r == 12) ? 0 : is_square_free($n >> 2)) if $r4 == 0;
      return is_square_free($n) if $r4 == 3;
    }
  }
  0;
}

my @_ds_overflow =  # We'll use BigInt math if the input is larger than this.
  (~0 > 4294967295)
   ? (124, 3000000000000000000, 3000000000, 2487240, 64260, 7026)
   : ( 50,           845404560,      52560,    1548,   252,   84);
sub divisor_sum {
  my($n, $k) = @_;
  return ((defined $k && $k==0) ? 2 : 1) if $n == 0;
  return 1 if $n == 1;

  if (defined $k && ref($k) eq 'CODE') {
    my $sum = $n-$n;
    my $refn = ref($n);
    foreach my $d (Math::Prime::Util::divisors($n)) {
      $sum += $k->( $refn ? $refn->new("$d") : $d );
    }
    return $sum;
  }

  croak "Second argument must be a code ref or number"
    unless !defined $k || _validate_num($k) || _validate_positive_integer($k);
  $k = 1 if !defined $k;

  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::sigma($n, $k))
    if $Math::Prime::Util::_GMPfunc{"sigma"};

  my $will_overflow = ($k == 0) ? (length($n) >= $_ds_overflow[0])
                    : ($k <= 5) ? ($n >= $_ds_overflow[$k])
                    : 1;

  # The standard way is:
  #    my $pk = $f ** $k;  $product *= ($pk ** ($e+1) - 1) / ($pk - 1);
  # But we get less overflow using:
  #    my $pk = $f ** $k;  $product *= $pk**E for E in 0 .. e
  # Also separate BigInt and do fiddly bits for better performance.

  my @factors = Math::Prime::Util::factor_exp($n);
  my $product = 1;
  my @fm;
  if ($k == 0) {
    $product = Math::Prime::Util::vecprod(map { $_->[1]+1 } @factors);
  } elsif (!$will_overflow) {
    foreach my $f (@factors) {
      my ($p, $e) = @$f;
      my $pk = $p ** $k;
      my $fmult = $pk + 1;
      foreach my $E (2 .. $e) { $fmult += $pk**$E }
      $product *= $fmult;
    }
  } elsif (ref($n) && ref($n) ne 'Math::BigInt') {
    # This can help a lot for Math::GMP, etc.
    $product = ref($n)->new(1);
    foreach my $f (@factors) {
      my ($p, $e) = @$f;
      my $pk = ref($n)->new($p) ** $k;
      my $fmult = $pk;  $fmult++;
      if ($e >= 2) {
        my $pke = $pk;
        for (2 .. $e) { $pke *= $pk; $fmult += $pke; }
      }
      $product *= $fmult;
    }
  } elsif ($k == 1) {
    foreach my $f (@factors) {
      my ($p, $e) = @$f;
      my $pk = Math::BigInt->new("$p");
      if ($e == 1) { push @fm, $pk->binc; next; }
      my $fmult = $pk->copy->binc;
      my $pke = $pk->copy;
      for my $E (2 .. $e) {
        $pke->bmul($pk);
        $fmult->badd($pke);
      }
      push @fm, $fmult;
    }
    $product = Math::Prime::Util::vecprod(@fm);
  } else {
    my $bik = Math::BigInt->new("$k");
    foreach my $f (@factors) {
      my ($p, $e) = @$f;
      my $pk = Math::BigInt->new("$p")->bpow($bik);
      if ($e == 1) { push @fm, $pk->binc; next; }
      my $fmult = $pk->copy->binc;
      my $pke = $pk->copy;
      for my $E (2 .. $e) {
        $pke->bmul($pk);
        $fmult->badd($pke);
      }
      push @fm, $fmult;
    }
    $product = Math::Prime::Util::vecprod(@fm);
  }
  $product;
}

#############################################################################
#                       Lehmer prime count
#
#my @_s0 = (0);
#my @_s1 = (0,1);
#my @_s2 = (0,1,1,1,1,2);
my @_s3 = (0,1,1,1,1,1,1,2,2,2,2,3,3,4,4,4,4,5,5,6,6,6,6,7,7,7,7,7,7,8);
my @_s4 = (0,1,1,1,1,1,1,1,1,1,1,2,2,3,3,3,3,4,4,5,5,5,5,6,6,6,6,6,6,7,7,8,8,8,8,8,8,9,9,9,9,10,10,11,11,11,11,12,12,12,12,12,12,13,13,13,13,13,13,14,14,15,15,15,15,15,15,16,16,16,16,17,17,18,18,18,18,18,18,19,19,19,19,20,20,20,20,20,20,21,21,21,21,21,21,21,21,22,22,22,22,23,23,24,24,24,24,25,25,26,26,26,26,27,27,27,27,27,27,27,27,28,28,28,28,28,28,29,29,29,29,30,30,30,30,30,30,31,31,32,32,32,32,33,33,33,33,33,33,34,34,35,35,35,35,35,35,36,36,36,36,36,36,37,37,37,37,38,38,39,39,39,39,40,40,40,40,40,40,41,41,42,42,42,42,42,42,43,43,43,43,44,44,45,45,45,45,46,46,47,47,47,47,47,47,47,47,47,47,48);
sub _tablephi {
  my($x, $a) = @_;
  if ($a == 0) { return $x; }
  elsif ($a == 1) { return $x-int($x/2); }
  elsif ($a == 2) { return $x-int($x/2) - int($x/3) + int($x/6); }
  elsif ($a == 3) { return  8 * int($x /  30) + $_s3[$x %  30]; }
  elsif ($a == 4) { return 48 * int($x / 210) + $_s4[$x % 210]; }
  elsif ($a == 5) { my $xp = int($x/11);
                    return ( (48 * int($x   / 210) + $_s4[$x   % 210]) -
                             (48 * int($xp  / 210) + $_s4[$xp  % 210]) ); }
  else            { my ($xp,$x2) = (int($x/11),int($x/13));
                    my $x2p = int($x2/11);
                    return ( (48 * int($x   / 210) + $_s4[$x   % 210]) -
                             (48 * int($xp  / 210) + $_s4[$xp  % 210]) -
                             (48 * int($x2  / 210) + $_s4[$x2  % 210]) +
                             (48 * int($x2p / 210) + $_s4[$x2p % 210]) ); }
}

sub legendre_phi {
  my ($x, $a, $primes) = @_;
  return _tablephi($x,$a) if $a <= 6;
  $primes = primes(Math::Prime::Util::nth_prime_upper($a+1)) unless defined $primes;
  return ($x > 0 ? 1 : 0) if $x < $primes->[$a];

  my $sum = 0;
  my %vals = ( $x => 1 );
  while ($a > 6) {
    my $primea = $primes->[$a-1];
    my %newvals;
    while (my($v,$c) = each %vals) {
      my $sval = int($v / $primea);
      if ($sval < $primea) {
        $sum -= $c;
      } else {
        $newvals{$sval} -= $c;
      }
    }
    # merge newvals into vals
    while (my($v,$c) = each %newvals) {
      $vals{$v} += $c;
      delete $vals{$v} if $vals{$v} == 0;
    }
    $a--;
  }
  while (my($v,$c) = each %vals) {
    $sum += $c * _tablephi($v, $a);
  }
  return $sum;
}

sub _sieve_prime_count {
  my $high = shift;
  return (0,0,1,2,2,3,3)[$high] if $high < 7;
  $high-- unless ($high & 1);
  return 1 + ${_sieve_erat($high)} =~ tr/0//;
}

sub _count_with_sieve {
  my ($sref, $low, $high) = @_;
  ($low, $high) = (2, $low) if !defined $high;
  my $count = 0;
  if   ($low < 3) { $low = 3; $count++; }
  else            { $low |= 1; }
  $high-- unless ($high & 1);
  return $count if $low > $high;
  my $sbeg = $low >> 1;
  my $send = $high >> 1;

  if ( !defined $sref || $send >= length($$sref) ) {
    # outside our range, so call the segment siever.
    my $seg_ref = _sieve_segment($low, $high);
    return $count + $$seg_ref =~ tr/0//;
  }
  return $count + substr($$sref, $sbeg, $send-$sbeg+1) =~ tr/0//;
}

sub _lehmer_pi {
  my $x = shift;
  return _sieve_prime_count($x) if $x < 1_000;
  do { require Math::BigFloat; Math::BigFloat->import(); }
    if ref($x) eq 'Math::BigInt';
  my $z = (ref($x) ne 'Math::BigInt')
        ? int(sqrt($x+0.5))
        : int(Math::BigFloat->new($x)->badd(0.5)->bsqrt->bfloor->bstr);
  my $a = _lehmer_pi(int(sqrt($z)+0.5));
  my $b = _lehmer_pi($z);
  my $c = _lehmer_pi(int( (ref($x) ne 'Math::BigInt')
                          ? $x**(1/3)+0.5
                          : Math::BigFloat->new($x)->broot(3)->badd(0.5)->bfloor
                     ));
  ($z, $a, $b, $c) = map { (ref($_) =~ /^Math::Big/) ? _bigint_to_int($_) : $_ }
                     ($z, $a, $b, $c);

  # Generate at least b primes.
  my $bth_prime_upper = ($b <= 10) ? 29 : int($b*(log($b) + log(log($b)))) + 1;
  my $primes = primes( $bth_prime_upper );

  my $sum = int(($b + $a - 2) * ($b - $a + 1) / 2);
  $sum += legendre_phi($x, $a, $primes);

  # Get a big sieve for our primecounts.  The C code compromises with either
  # b*10 or x^3/5, as that cuts out all the inner loop sieves and about half
  # of the big outer loop counts.
  # Our sieve count isn't nearly as optimized here, so error on the side of
  # more primes.  This uses a lot more memory but saves a lot of time.
  my $sref = _sieve_erat( int($x / $primes->[$a] / 5) );

  my ($lastw, $lastwpc) = (0,0);
  foreach my $i (reverse $a+1 .. $b) {
    my $w = int($x / $primes->[$i-1]);
    $lastwpc += _count_with_sieve($sref,$lastw+1, $w);
    $lastw = $w;
    $sum -= $lastwpc;
    #$sum -= _count_with_sieve($sref,$w);
    if ($i <= $c) {
      my $bi = _count_with_sieve($sref,int(sqrt($w)+0.5));
      foreach my $j ($i .. $bi) {
        $sum = $sum - _count_with_sieve($sref,int($w / $primes->[$j-1])) + $j - 1;
      }
    }
  }
  $sum;
}
#############################################################################


sub prime_count {
  my($low,$high) = @_;
  if (!defined $high) {
    $high = $low;
    $low = 2;
  }
  _validate_positive_integer($low);
  _validate_positive_integer($high);

  my $count = 0;

  $count++ if ($low <= 2) && ($high >= 2);   # Count 2
  $low = 3 if $low < 3;

  $low++ if ($low % 2) == 0;   # Make low go to odd number.
  $high-- if ($high % 2) == 0; # Make high go to odd number.
  return $count if $low > $high;

  if (   ref($low) eq 'Math::BigInt' || ref($high) eq 'Math::BigInt'
      || ($high-$low) < 10
      || ($high-$low) < int($low/100_000_000_000) ) {
    # Trial primes seems best.  Needs some tuning.
    my $curprime = next_prime($low-1);
    while ($curprime <= $high) {
      $count++;
      $curprime = next_prime($curprime);
    }
    return $count;
  }

  # TODO: Needs tuning
  if ($high > 50_000) {
    if ( ($high / ($high-$low+1)) < 100 ) {
      $count += _lehmer_pi($high);
      $count -= ($low == 3) ? 1 : _lehmer_pi($low-1);
      return $count;
    }
  }

  return (_sieve_prime_count($high) - 1 + $count) if $low == 3;

  my $sieveref = _sieve_segment($low,$high);
  $count += $$sieveref =~ tr/0//;
  return $count;
}


sub nth_prime {
  my($n) = @_;
  _validate_positive_integer($n);

  return undef if $n <= 0;  ## no critic qw(ProhibitExplicitReturnUndef)
  return $_primes_small[$n] if $n <= $#_primes_small;

  if ($n > MPU_MAXPRIMEIDX && ref($n) ne 'Math::BigFloat') {
    do { require Math::BigFloat; Math::BigFloat->import(); }
      if !defined $Math::BigFloat::VERSION;
    $n = Math::BigFloat->new("$n")
  }

  my $prime = 0;
  my $count = 1;
  my $start = 3;

  my $logn = log($n);
  my $loglogn = log($logn);
  my $nth_prime_upper = ($n <= 10) ? 29 : int($n*($logn + $loglogn)) + 1;
  if ($nth_prime_upper > 100000) {
    # Use fast Lehmer prime count combined with lower bound to get close.
    my $nth_prime_lower = int($n * ($logn + $loglogn - 1.0 + (($loglogn-2.10)/$logn)));
    $nth_prime_lower-- unless $nth_prime_lower % 2;
    $count = _lehmer_pi($nth_prime_lower);
    $start = $nth_prime_lower + 2;
  }

  {
    # Make sure incr is an even number.
    my $incr = ($n < 1000) ? 1000 : ($n < 10000) ? 10000 : 100000;
    my $sieveref;
    while (1) {
      $sieveref = _sieve_segment($start, $start+$incr);
      my $segcount = $$sieveref =~ tr/0//;
      last if ($count + $segcount) >= $n;
      $count += $segcount;
      $start += $incr+2;
    }
    # Our count is somewhere in this segment.  Need to look for it.
    $prime = $start - 2;
    while ($count < $n) {
      $prime += 2;
      $count++ if !substr($$sieveref, ($prime-$start)>>1, 1);
    }
  }
  $prime;
}

# The nth prime will be less or equal to this number
sub nth_prime_upper {
  my($n) = @_;
  _validate_positive_integer($n);

  return undef if $n <= 0;  ## no critic qw(ProhibitExplicitReturnUndef)
  return $_primes_small[$n] if $n <= $#_primes_small;

  $n = _upgrade_to_float($n) if $n > MPU_MAXPRIMEIDX || $n > 2**45;

  my $flogn  = log($n);
  my $flog2n = log($flogn);  # Note distinction between log_2(n) and log^2(n)

  my $upper;
  if      ($n >= 46254381) {  # Axler 2017 Corollary 1.2
    $upper = $n * ( $flogn  +  $flog2n-1.0  +  (($flog2n-2.00)/$flogn)  -  (($flog2n*$flog2n - 6*$flog2n + 10.667)/(2*$flogn*$flogn)) );
  } elsif ($n >=  8009824) {  # Axler 2013 page viii Korollar G
    $upper = $n * ( $flogn  +  $flog2n-1.0  +  (($flog2n-2.00)/$flogn)  -  (($flog2n*$flog2n - 6*$flog2n + 10.273)/(2*$flogn*$flogn)) );
  } elsif ($n >=  688383) {   # Dusart 2010 page 2
    $upper = $n * ( $flogn  +  $flog2n - 1.0 + (($flog2n-2.00)/$flogn) );
  } elsif ($n >=  178974) {   # Dusart 2010 page 7
    $upper = $n * ( $flogn  +  $flog2n - 1.0 + (($flog2n-1.95)/$flogn) );
  } elsif ($n >=   39017) {   # Dusart 1999 page 14
    $upper = $n * ( $flogn  +  $flog2n - 0.9484 );
  } elsif ($n >=       6) {   # Modified Robin 1983, for 6-39016 only
    $upper = $n * ( $flogn  +  0.6000 * $flog2n );
  } else {
    $upper = $n * ( $flogn  +  $flog2n );
  }

  return int($upper + 1.0);
}

# The nth prime will be greater than or equal to this number
sub nth_prime_lower {
  my($n) = @_;
  _validate_num($n) || _validate_positive_integer($n);

  return undef if $n <= 0;  ## no critic qw(ProhibitExplicitReturnUndef)
  return $_primes_small[$n] if $n <= $#_primes_small;

  $n = _upgrade_to_float($n) if $n > MPU_MAXPRIMEIDX || $n > 2**45;

  my $flogn  = log($n);
  my $flog2n = log($flogn);  # Note distinction between log_2(n) and log^2(n)

  # Dusart 1999 page 14, for all n >= 2
  #my $lower = $n * ($flogn + $flog2n - 1.0 + (($flog2n-2.25)/$flogn));
  # Dusart 2010 page 2, for all n >= 3
  #my $lower = $n * ($flogn + $flog2n - 1.0 + (($flog2n-2.10)/$flogn));
  # Axler 2013 page viii Korollar I, for all n >= 2
  #my $lower = $n * ($flogn + $flog2n-1.0 + (($flog2n-2.00)/$flogn) - (($flog2n*$flog2n - 6*$flog2n + 11.847)/(2*$flogn*$flogn)) );
  # Axler 2017 Corollary 1.4
  my $lower = $n * ($flogn + $flog2n-1.0 + (($flog2n-2.00)/$flogn) - (($flog2n*$flog2n - 6*$flog2n + 11.508)/(2*$flogn*$flogn)) );

  return int($lower + 0.999999999);
}

sub inverse_li {
  my($n) = @_;
  _validate_num($n) || _validate_positive_integer($n);

  return (0,2,3,5,6,8)[$n] if $n <= 5;
  $n = _upgrade_to_float($n) if $n > MPU_MAXPRIMEIDX || $n > 2**45;
  my $t = $n * log($n);

  # Iterator Halley's method until error term grows
  my $old_term = MPU_INFINITY;
  for my $iter (1 .. 10000) {
    my $dn = Math::Prime::Util::LogarithmicIntegral($t) - $n;
    my $term = $dn * log($t) / (1.0 + $dn/(2*$t));
    last if abs($term) >= abs($old_term);
    $old_term = $term;
    $t -= $term;
    last if abs($term) < 1e-6;
  }
  if (ref($t)) {
    $t = Math::BigInt->new($t->bceil->bstr);
    $t = _bigint_to_int($t) if $t->bacmp(BMAX) <= 0;
  } else {
    $t = int($t+0.999999);
  }
  $t;
}
sub _inverse_R {
  my($n) = @_;
  _validate_num($n) || _validate_positive_integer($n);

  return (0,2,3,5,6,8)[$n] if $n <= 5;
  $n = _upgrade_to_float($n) if $n > MPU_MAXPRIMEIDX || $n > 2**45;
  my $t = $n * log($n);

  # Iterator Halley's method until error term grows
  my $old_term = MPU_INFINITY;
  for my $iter (1 .. 10000) {
    my $dn = Math::Prime::Util::RiemannR($t) - $n;
    my $term = $dn * log($t) / (1.0 + $dn/(2*$t));
    last if abs($term) >= abs($old_term);
    $old_term = $term;
    $t -= $term;
    last if abs($term) < 1e-6;
  }
  if (ref($t)) {
    $t = Math::BigInt->new($t->bceil->bstr);
    $t = _bigint_to_int($t) if $t->bacmp(BMAX) <= 0;
  } else {
    $t = int($t+0.999999);
  }
  $t;
}

sub nth_prime_approx {
  my($n) = @_;
  _validate_num($n) || _validate_positive_integer($n);

  return undef if $n <= 0;  ## no critic qw(ProhibitExplicitReturnUndef)
  return $_primes_small[$n] if $n <= $#_primes_small;

  # Once past 10^12 or so, inverse_li gives better results.
  return Math::Prime::Util::inverse_li($n) if $n > 1e12;

  $n = _upgrade_to_float($n)
    if ref($n) eq 'Math::BigInt' || $n >= MPU_MAXPRIMEIDX;

  my $flogn  = log($n);
  my $flog2n = log($flogn);

  # Cipolla 1902:
  #    m=0   fn * ( flogn + flog2n - 1 );
  #    m=1   + ((flog2n - 2)/flogn) );
  #    m=2   - (((flog2n*flog2n) - 6*flog2n + 11) / (2*flogn*flogn))
  #    + O((flog2n/flogn)^3)
  #
  # Shown in Dusart 1999 page 12, as well as other sources such as:
  #   http://www.emis.de/journals/JIPAM/images/153_02_JIPAM/153_02.pdf
  # where the main issue you run into is that you're doing polynomial
  # interpolation, so it oscillates like crazy with many high-order terms.
  # Hence I'm leaving it at m=2.

  my $approx = $n * ( $flogn + $flog2n - 1
                      + (($flog2n - 2)/$flogn)
                      - ((($flog2n*$flog2n) - 6*$flog2n + 11) / (2*$flogn*$flogn))
                    );

  # Apply a correction to help keep values close.
  my $order = $flog2n/$flogn;
  $order = $order*$order*$order * $n;

  if    ($n <        259) { $approx += 10.4 * $order; }
  elsif ($n <        775) { $approx +=  6.3 * $order; }
  elsif ($n <       1271) { $approx +=  5.3 * $order; }
  elsif ($n <       2000) { $approx +=  4.7 * $order; }
  elsif ($n <       4000) { $approx +=  3.9 * $order; }
  elsif ($n <      12000) { $approx +=  2.8 * $order; }
  elsif ($n <     150000) { $approx +=  1.2 * $order; }
  elsif ($n <   20000000) { $approx +=  0.11 * $order; }
  elsif ($n <  100000000) { $approx +=  0.008 * $order; }
  elsif ($n <  500000000) { $approx += -0.038 * $order; }
  elsif ($n < 2000000000) { $approx += -0.054 * $order; }
  else                    { $approx += -0.058 * $order; }
  # If we want the asymptotic approximation to be >= actual, use -0.010.

  return int($approx + 0.5);
}

#############################################################################

sub prime_count_approx {
  my($x) = @_;
  _validate_num($x) || _validate_positive_integer($x);

  # Turn on high precision FP if they gave us a big number.
  $x = _upgrade_to_float($x) if ref($_[0]) eq 'Math::BigInt' && $x > 1e16;
  #    Method             10^10 %error  10^19 %error
  #    -----------------  ------------  ------------
  #    n/(log(n)-1)        .22%          .058%
  #    n/(ln(n)-1-1/ln(n)) .032%         .0041%
  #    average bounds      .0005%        .0000002%
  #    asymp               .0006%        .00000004%
  #    li(n)               .0007%        .00000004%
  #    li(n)-li(n^.5)/2    .0004%        .00000001%
  #    R(n)                .0004%        .00000001%
  #
  # Also consider: http://trac.sagemath.org/sage_trac/ticket/8135

  # Asymp:
  #   my $l1 = log($x);  my $l2 = $l1*$l1;  my $l4 = $l2*$l2;
  #   my $result = int( $x/$l1 + $x/$l2 + 2*$x/($l2*$l1) + 6*$x/($l4) + 24*$x/($l4*$l1) + 120*$x/($l4*$l2) + 720*$x/($l4*$l2*$l1) + 5040*$x/($l4*$l4) + 40320*$x/($l4*$l4*$l1) + 0.5 );
  # my $result = int( (prime_count_upper($x) + prime_count_lower($x)) / 2);
  # my $result = int( LogarithmicIntegral($x) );
  # my $result = int(LogarithmicIntegral($x) - LogarithmicIntegral(sqrt($x))/2);
  # my $result = RiemannR($x) + 0.5;

  # Make sure we get enough accuracy, and also not too much more than needed
  $x->accuracy(length($x->copy->as_int->bstr())+2) if ref($x) =~ /^Math::Big/;

  my $result;
  if ($Math::Prime::Util::_GMPfunc{"riemannr"} || !ref($x)) {
    # Fast if we have our GMP backend, and ok for native.
    $result = Math::Prime::Util::PP::RiemannR($x);
  } else {
    $x = _upgrade_to_float($x) unless ref($x) eq 'Math::BigFloat';
    $result = Math::BigFloat->new(0);
    $result->accuracy($x->accuracy) if ref($x) && $x->accuracy;
    $result += Math::BigFloat->new(LogarithmicIntegral($x));
    $result -= Math::BigFloat->new(LogarithmicIntegral(sqrt($x))/2);
    my $intx = ref($x) ? Math::BigInt->new($x->bfround(0)) : $x;
    for my $k (3 .. 1000) {
      my $m = moebius($k);
      next unless $m != 0;
      # With Math::BigFloat and the Calc backend, FP root is ungodly slow.
      # Use integer root instead.  For more accuracy (not useful here):
      # my $v = Math::BigFloat->new( "" . rootint($x->as_int,$k) );
      # $v->accuracy(length($v)+5);
      # $v = $v - Math::BigFloat->new(($v**$k - $x))->bdiv($k * $v**($k-1));
      # my $term = LogarithmicIntegral($v)/$k;
      my $term = LogarithmicIntegral(rootint($intx,$k)) / $k;
      last if $term < .25;
      if ($m == 1) { $result->badd(Math::BigFloat->new($term)) }
      else         { $result->bsub(Math::BigFloat->new($term)) }
    }
  }

  if (ref($result)) {
    return $result unless ref($result) eq 'Math::BigFloat';
    # Math::BigInt::FastCalc 0.19 implements as_int incorrectly.
    return Math::BigInt->new($result->bfround(0)->bstr);
  }
  int($result+0.5);
}

sub prime_count_lower {
  my($x) = @_;
  _validate_num($x) || _validate_positive_integer($x);

  return _tiny_prime_count($x) if $x < $_primes_small[-1];

  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::prime_count_lower($x))
    if $Math::Prime::Util::_GMPfunc{"prime_count_lower"};

  $x = _upgrade_to_float($x)
    if ref($x) eq 'Math::BigInt' || ref($_[0]) eq 'Math::BigInt';

  my($result,$a);
  my $fl1 = log($x);
  my $fl2 = $fl1*$fl1;
  my $one = (ref($x) eq 'Math::BigFloat') ? $x->copy->bone : $x-$x+1.0;

  # Chebyshev            1*x/logx       x >= 17
  # Rosser & Schoenfeld  x/(logx-1/2)   x >= 67
  # Dusart 1999          x/logx*(1+1/logx+1.8/logxlogx)  x >= 32299
  # Dusart 2010          x/logx*(1+1/logx+2.0/logxlogx)  x >= 88783
  # Axler 2014 (1.2)     ""+...                          x >= 1332450001
  # Axler 2014 (1.2)     x/(logx-1-1/logx-...)           x >= 1332479531
  # Büthe 2015 (1.9)     li(x)-(sqrtx/logx)*(...)        x <= 10^19
  # Büthe 2014 Th 2      li(x)-logx*sqrtx/8Pi    x > 2657, x <= 1.4*10^25

  if ($x < 599) {                         # Decent for small numbers
    $result = $x / ($fl1 - 0.7);
  } elsif ($x < 52600000) {               # Dusart 2010 tweaked
    if    ($x <       2700) { $a = 0.30; }
    elsif ($x <       5500) { $a = 0.90; }
    elsif ($x <      19400) { $a = 1.30; }
    elsif ($x <      32299) { $a = 1.60; }
    elsif ($x <      88783) { $a = 1.83; }
    elsif ($x <     176000) { $a = 1.99; }
    elsif ($x <     315000) { $a = 2.11; }
    elsif ($x <    1100000) { $a = 2.19; }
    elsif ($x <    4500000) { $a = 2.31; }
    else                    { $a = 2.35; }
    $result = ($x/$fl1) * ($one + $one/$fl1 + $a/$fl2);
  } elsif ($x < 1.4e25 || Math::Prime::Util::prime_get_config()->{'assume_rh'}){
                                          # Büthe 2014/2015
    my $lix = LogarithmicIntegral($x);
    my $sqx = sqrt($x);
    if ($x < 1e19) {
      $result = $lix - ($sqx/$fl1) * (1.94 + 3.88/$fl1 + 27.57/$fl2);
    } else {
      if (ref($x) eq 'Math::BigFloat') {
        my $xdigits = _find_big_acc($x);
        $result = $lix - ($fl1*$sqx / (Math::BigFloat->bpi($xdigits)*8));
      } else {
        $result = $lix - ($fl1*$sqx / PI_TIMES_8);
      }
    }
  } else {                                # Axler 2014 1.4
    my($fl3,$fl4) = ($fl2*$fl1,$fl2*$fl2);
    my($fl5,$fl6) = ($fl4*$fl1,$fl4*$fl2);
    $result = $x / ($fl1 - $one - $one/$fl1 - 2.65/$fl2 - 13.35/$fl3 - 70.3/$fl4 - 455.6275/$fl5 - 3404.4225/$fl6);
  }

  return Math::BigInt->new($result->bfloor->bstr()) if ref($result) eq 'Math::BigFloat';
  return int($result);
}

sub prime_count_upper {
  my($x) = @_;
  _validate_num($x) || _validate_positive_integer($x);

  # Give an exact answer for what we have in our little table.
  return _tiny_prime_count($x) if $x < $_primes_small[-1];

  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::prime_count_upper($x))
    if $Math::Prime::Util::_GMPfunc{"prime_count_upper"};

  $x = _upgrade_to_float($x)
    if ref($x) eq 'Math::BigInt' || ref($_[0]) eq 'Math::BigInt';

  # Chebyshev:            1.25506*x/logx       x >= 17
  # Rosser & Schoenfeld:  x/(logx-3/2)         x >= 67
  # Panaitopol 1999:      x/(logx-1.112)       x >= 4
  # Dusart 1999:          x/logx*(1+1/logx+2.51/logxlogx)   x >= 355991
  # Dusart 2010:          x/logx*(1+1/logx+2.334/logxlogx)  x >= 2_953_652_287
  # Axler 2014:           x/(logx-1-1/logx-3.35/logxlogx...) x >= e^3.804
  # Büthe 2014 7.4        Schoenfeld bounds hold to x <= 1.4e25
  # Axler 2017 Prop 2.2   Schoenfeld bounds hold to x <= 5.5e25
  # Skewes                li(x)                x < 1e14

  my($result,$a);
  my $fl1 = log($x);
  my $fl2 = $fl1 * $fl1;
  my $one = (ref($x) eq 'Math::BigFloat') ? $x->copy->bone : $x-$x+1.0;

  if ($x < 15900) {              # Tweaked Rosser-type
    $a = ($x < 1621) ? 1.048 : ($x < 5000) ? 1.071 : 1.098;
    $result = ($x / ($fl1 - $a)) + 1.0;
  } elsif ($x < 821800000) {     # Tweaked Dusart 2010
    if    ($x <      24000) { $a = 2.30; }
    elsif ($x <      59000) { $a = 2.48; }
    elsif ($x <     350000) { $a = 2.52; }
    elsif ($x <     355991) { $a = 2.54; }
    elsif ($x <     356000) { $a = 2.51; }
    elsif ($x <    3550000) { $a = 2.50; }
    elsif ($x <    3560000) { $a = 2.49; }
    elsif ($x <    5000000) { $a = 2.48; }
    elsif ($x <    8000000) { $a = 2.47; }
    elsif ($x <   13000000) { $a = 2.46; }
    elsif ($x <   18000000) { $a = 2.45; }
    elsif ($x <   31000000) { $a = 2.44; }
    elsif ($x <   41000000) { $a = 2.43; }
    elsif ($x <   48000000) { $a = 2.42; }
    elsif ($x <  119000000) { $a = 2.41; }
    elsif ($x <  182000000) { $a = 2.40; }
    elsif ($x <  192000000) { $a = 2.395; }
    elsif ($x <  213000000) { $a = 2.390; }
    elsif ($x <  271000000) { $a = 2.385; }
    elsif ($x <  322000000) { $a = 2.380; }
    elsif ($x <  400000000) { $a = 2.375; }
    elsif ($x <  510000000) { $a = 2.370; }
    elsif ($x <  682000000) { $a = 2.367; }
    elsif ($x < 2953652287) { $a = 2.362; }
    else                    { $a = 2.334; } # Dusart 2010, page 2
    $result = ($x/$fl1) * ($one + $one/$fl1 + $a/$fl2) + $one;
  } elsif ($x < 1e19) {                     # Skewes number lower limit
    $a = ($x < 110e7) ? 0.032 : ($x < 1001e7) ? 0.027 : ($x < 10126e7) ? 0.021 : 0.0;
    $result = LogarithmicIntegral($x) - $a * $fl1*sqrt($x)/PI_TIMES_8;
  } elsif ($x < 5.5e25 || Math::Prime::Util::prime_get_config()->{'assume_rh'}) {
                                            # Schoenfeld / Büthe 2014 Th 7.4
    my $lix = LogarithmicIntegral($x);
    my $sqx = sqrt($x);
    if (ref($x) eq 'Math::BigFloat') {
      my $xdigits = _find_big_acc($x);
      $result = $lix + ($fl1*$sqx / (Math::BigFloat->bpi($xdigits)*8));
    } else {
      $result = $lix + ($fl1*$sqx / PI_TIMES_8);
    }
  } else {                                  # Axler 2014 1.3
    my($fl3,$fl4) = ($fl2*$fl1,$fl2*$fl2);
    my($fl5,$fl6) = ($fl4*$fl1,$fl4*$fl2);
    $result = $x / ($fl1 - $one - $one/$fl1 - 3.35/$fl2 - 12.65/$fl3 - 71.7/$fl4 - 466.1275/$fl5 - 3489.8225/$fl6);
  }

  return Math::BigInt->new($result->bfloor->bstr()) if ref($result) eq 'Math::BigFloat';
  return int($result);
}

sub twin_prime_count {
  my($low,$high) = @_;
  if (defined $high) { _validate_positive_integer($low); }
  else               { ($low,$high) = (2, $low);         }
  _validate_positive_integer($high);
  my $sum = 0;
  while ($low <= $high) {
    my $seghigh = ($high-$high) + $low + 1e7 - 1;
    $seghigh = $high if $seghigh > $high;
    $sum += scalar(@{Math::Prime::Util::twin_primes($low,$seghigh)});
    $low = $seghigh + 1;
  }
  $sum;
}
sub _semiprime_count {
  my $n = shift;
  my($sum,$pc) = (0,0);
  Math::Prime::Util::forprimes( sub {
    $sum += Math::Prime::Util::prime_count(int($n/$_))-$pc++;
  }, sqrtint($n));
  $sum;
}
sub semiprime_count {
  my($low,$high) = @_;
  if (defined $high) { _validate_positive_integer($low); }
  else               { ($low,$high) = (2, $low);         }
  _validate_positive_integer($high);
  # todo: threshold of fast count vs. walk
  my $sum = _semiprime_count($high) - (($low < 4) ? 0 : semiprime_count($low-1));
  $sum;
}
sub ramanujan_prime_count {
  my($low,$high) = @_;
  if (defined $high) { _validate_positive_integer($low); }
  else               { ($low,$high) = (2, $low);         }
  _validate_positive_integer($high);
  my $sum = 0;
  while ($low <= $high) {
    my $seghigh = ($high-$high) + $low + 1e9 - 1;
    $seghigh = $high if $seghigh > $high;
    $sum += scalar(@{Math::Prime::Util::ramanujan_primes($low,$seghigh)});
    $low = $seghigh + 1;
  }
  $sum;
}

sub twin_prime_count_approx {
  my($n) = @_;
  return twin_prime_count(3,$n) if $n < 2000;
  $n = _upgrade_to_float($n) if ref($n);
  my $logn = log($n);
  # The loss of full Ei precision is a few orders of magnitude less than the
  # accuracy of the estimate, so save huge time and don't bother.
  my $li2 = Math::Prime::Util::ExponentialIntegral("$logn") + 2.8853900817779268147198494 - ($n/$logn);

  # Empirical correction factor
  my $fm;
  if    ($n <     4000) { $fm = 0.2952; }
  elsif ($n <     8000) { $fm = 0.3151; }
  elsif ($n <    16000) { $fm = 0.3090; }
  elsif ($n <    32000) { $fm = 0.3096; }
  elsif ($n <    64000) { $fm = 0.3100; }
  elsif ($n <   128000) { $fm = 0.3089; }
  elsif ($n <   256000) { $fm = 0.3099; }
  elsif ($n <   600000) { my($x0, $x1, $y0, $y1) = (1e6, 6e5, .3091, .3059);
                          $fm = $y0 + ($n - $x0) * ($y1-$y0) / ($x1 - $x0); }
  elsif ($n <  1000000) { my($x0, $x1, $y0, $y1) = (6e5, 1e6, .3062, .3042);
                          $fm = $y0 + ($n - $x0) * ($y1-$y0) / ($x1 - $x0); }
  elsif ($n <  4000000) { my($x0, $x1, $y0, $y1) = (1e6, 4e6, .3067, .3041);
                          $fm = $y0 + ($n - $x0) * ($y1-$y0) / ($x1 - $x0); }
  elsif ($n < 16000000) { my($x0, $x1, $y0, $y1) = (4e6, 16e6, .3033, .2983);
                          $fm = $y0 + ($n - $x0) * ($y1-$y0) / ($x1 - $x0); }
  elsif ($n < 32000000) { my($x0, $x1, $y0, $y1) = (16e6, 32e6, .2980, .2965);
                          $fm = $y0 + ($n - $x0) * ($y1-$y0) / ($x1 - $x0); }
  $li2 *= $fm * log(12+$logn)  if defined $fm;

  return int(1.32032363169373914785562422 * $li2 + 0.5);
}

sub semiprime_count_approx {
  my($n) = @_;
  return 0 if $n < 4;
  _validate_positive_integer($n);
  $n = "$n" + 0.00000001;
  my $l1 = log($n);
  my $l2 = log($l1);
  #my $est = $n * $l2 / $l1;
  my $est = $n * ($l2 + 0.302) / $l1;
  int(0.5+$est);
}

sub nth_twin_prime {
  my($n) = @_;
  return undef if $n < 0;  ## no critic qw(ProhibitExplicitReturnUndef)
  return (undef,3,5,11,17,29,41)[$n] if $n <= 6;

  my $p = Math::Prime::Util::nth_twin_prime_approx($n+200);
  my $tp = Math::Prime::Util::twin_primes($p);
  while ($n > scalar(@$tp)) {
    $n -= scalar(@$tp);
    $tp = Math::Prime::Util::twin_primes($p+1,$p+1e5);
    $p += 1e5;
  }
  return $tp->[$n-1];
}

sub nth_twin_prime_approx {
  my($n) = @_;
  _validate_positive_integer($n);
  return nth_twin_prime($n) if $n < 6;
  $n = _upgrade_to_float($n) if ref($n) || $n > 127e14;   # TODO lower for 32-bit
  my $logn = log($n);
  my $nlogn2 = $n * $logn * $logn;

  return int(5.158 * $nlogn2/log(9+log($n*$n))) if $n > 59 && $n <= 1092;

  my $lo = int(0.7 * $nlogn2);
  my $hi = int( ($n > 1e16) ? 1.1 * $nlogn2
              : ($n >  480) ? 1.7 * $nlogn2
                            : 2.3 * $nlogn2 + 3 );

  _binary_search($n, $lo, $hi,
                 sub{Math::Prime::Util::twin_prime_count_approx(shift)},
                 sub{ ($_[2]-$_[1])/$_[1] < 1e-15 } );
}

sub nth_semiprime {
  my $n = shift;
  return undef if $n < 0;  ## no critic qw(ProhibitExplicitReturnUndef)
  return (undef,4,6,9,10,14,15,21,22)[$n] if $n <= 8;
  my $logn = log($n);
  my $est = 0.966 * $n * $logn / log($logn);
  1+_binary_search($n, int(0.9*$est)-1, int(1.15*$est)+1,
                   sub{Math::Prime::Util::semiprime_count(shift)});
}

sub nth_semiprime_approx {
  my $n = shift;
  return undef if $n < 0;  ## no critic qw(ProhibitExplicitReturnUndef)
  _validate_positive_integer($n);
  return (undef,4,6,9,10,14,15,21,22)[$n] if $n <= 8;
  $n = "$n" + 0.00000001;
  my $l1 = log($n);
  my $l2 = log($l1);
  my $est = 0.966 * $n * $l1 / $l2;
  int(0.5+$est);
}

sub nth_ramanujan_prime_upper {
  my $n = shift;
  return (0,2,11)[$n] if $n <= 2;
  $n = Math::BigInt->new("$n") if $n > (~0/3);
  my $nth = nth_prime_upper(3*$n);
  return $nth if $n < 10000;
  $nth = Math::BigInt->new("$nth") if $nth > (~0/177);
  if ($n < 1000000) { $nth = (177 * $nth) >> 8; }
  elsif ($n < 1e10) { $nth = (175 * $nth) >> 8; }
  else              { $nth = (133 * $nth) >> 8; }
  $nth = _bigint_to_int($nth) if ref($nth) && $nth->bacmp(BMAX) <= 0;
  $nth;
}
sub nth_ramanujan_prime_lower {
  my $n = shift;
  return (0,2,11)[$n] if $n <= 2;
  $n = Math::BigInt->new("$n") if $n > (~0/2);
  my $nth = nth_prime_lower(2*$n);
  $nth = Math::BigInt->new("$nth") if $nth > (~0/275);
  if ($n < 10000)   { $nth = (275 * $nth) >> 8; }
  elsif ($n < 1e10) { $nth = (262 * $nth) >> 8; }
  $nth = _bigint_to_int($nth) if ref($nth) && $nth->bacmp(BMAX) <= 0;
  $nth;
}
sub nth_ramanujan_prime_approx {
  my $n = shift;
  return (0,2,11)[$n] if $n <= 2;
  my($lo,$hi) = (nth_ramanujan_prime_lower($n),nth_ramanujan_prime_upper($n));
  $lo + (($hi-$lo)>>1);
}
sub ramanujan_prime_count_upper {
  my $n = shift;
  return (($n < 2) ? 0 : 1) if $n < 11;
  my $lo = int(prime_count_lower($n) / 3);
  my $hi = prime_count_upper($n) >> 1;
  1+_binary_search($n, $lo, $hi,
                   sub{Math::Prime::Util::nth_ramanujan_prime_lower(shift)});
}
sub ramanujan_prime_count_lower {
  my $n = shift;
  return (($n < 2) ? 0 : 1) if $n < 11;
  my $lo = int(prime_count_lower($n) / 3);
  my $hi = prime_count_upper($n) >> 1;
  _binary_search($n, $lo, $hi,
                 sub{Math::Prime::Util::nth_ramanujan_prime_upper(shift)});
}
sub ramanujan_prime_count_approx {
  my $n = shift;
  return (($n < 2) ? 0 : 1) if $n < 11;
  #$n = _upgrade_to_float($n) if ref($n) || $n > 2e16;
  my $lo = ramanujan_prime_count_lower($n);
  my $hi = ramanujan_prime_count_upper($n);
  _binary_search($n, $lo, $hi,
                 sub{Math::Prime::Util::nth_ramanujan_prime_approx(shift)},
                 sub{ ($_[2]-$_[1])/$_[1] < 1e-15 } );
}

sub _sum_primes_n {
  my $n = shift;
  return (0,0,2,5,5)[$n] if $n < 5;
  my $r = Math::Prime::Util::sqrtint($n);
  my $r2 = $r + int($n/($r+1));
  my(@V,@S);
  for my $k (0 .. $r2) {
    my $v = ($k <= $r) ? $k : int($n/($r2-$k+1));
    $V[$k] = $v;
    $S[$k] = (($v*($v+1)) >> 1) - 1;
  }
  Math::Prime::Util::forprimes( sub { my $p = $_;
    my $sp = $S[$p-1];
    my $p2 = $p*$p;
    for my $v (reverse @V) {
      last if $v < $p2;
      my($a,$b) = ($v,int($v/$p));
      $a = $r2 - int($n/$a) + 1 if $a > $r;
      $b = $r2 - int($n/$b) + 1 if $b > $r;
      $S[$a] -= $p * ($S[$b] - $sp);
    }
  }, 2, $r);
  $S[$r2];
}

sub sum_primes {
  my($low,$high) = @_;
  if (defined $high) { _validate_positive_integer($low); }
  else               { ($low,$high) = (2, $low);         }
  _validate_positive_integer($high);
  my $sum = 0;
  $sum = BZERO->copy if ( (MPU_32BIT && $high >        323_380) ||
                          (MPU_64BIT && $high > 29_505_444_490) );

  # It's very possible we're here because they've counted too high.  Skip fwd.
  if ($low <= 2 && $high >= 29505444491) {
    $low = 29505444503;
    $sum = Math::BigInt->new("18446744087046669523");
  }

  return $sum if $low > $high;

  # We have to make some decision about whether to use our PP prime sum or loop
  # doing the XS sieve.  TODO: Be smarter here?
  if (!Math::Prime::Util::prime_get_config()->{'xs'} && !ref($sum) && !MPU_32BIT && ($high-$low) > 1000000) {
    # Unfortunately with bigints this is horrifically slow, but we have to do it.
    $high = BZERO->copy + $high  if $high >= (1 << (MPU_MAXBITS/2))-1;
    $sum = _sum_primes_n($high);
    $sum -= _sum_primes_n($low-1) if $low > 2;
    return $sum;
  }

  my $xssum = (MPU_64BIT && $high < 6e14 && Math::Prime::Util::prime_get_config()->{'xs'});
  my $step = ($xssum && $high > 5e13) ? 1_000_000 : 11_000_000;
  Math::Prime::Util::prime_precalc(sqrtint($high));
  while ($low <= $high) {
    my $next = $low + $step - 1;
    $next = $high if $next > $high;
    $sum += ($xssum) ? Math::Prime::Util::sum_primes($low,$next)
                     : Math::Prime::Util::vecsum( @{Math::Prime::Util::primes($low,$next)} );
    last if $next == $high;
    $low = $next+1;
  }
  $sum;
}
sub print_primes {
  my($low,$high,$fd) = @_;
  if (defined $high) { _validate_positive_integer($low); }
  else               { ($low,$high) = (2, $low);         }
  _validate_positive_integer($high);

  $fd = fileno(STDOUT) unless defined $fd;
  open(my $fh, ">>&=", $fd);  # TODO .... or die

  if ($high >= $low) {
    my $p1 = $low;
    while ($p1 <= $high) {
      my $p2 = $p1 + 15_000_000 - 1;
      $p2 = $high if $p2 > $high;
      if ($Math::Prime::Util::_GMPfunc{"sieve_primes"}) {
        print $fh "$_\n" for Math::Prime::Util::GMP::sieve_primes($p1,$p2,0);
      } else {
        print $fh "$_\n" for @{primes($p1,$p2)};
      }
      $p1 = $p2+1;
    }
  }
  close($fh);
}


#############################################################################

sub _mulmod {
  my($x, $y, $n) = @_;
  return (($x * $y) % $n) if ($x|$y) < MPU_HALFWORD;
  #return (($x * $y) % $n) if ($x|$y) < MPU_HALFWORD || $y == 0 || $x < int(~0/$y);
  my $r = 0;
  $x %= $n if $x >= $n;
  $y %= $n if $y >= $n;
  ($x,$y) = ($y,$x) if $x < $y;
  if ($n <= (~0 >> 1)) {
    while ($y > 1) {
      if ($y & 1) { $r += $x;  $r -= $n if $r >= $n; }
      $y >>= 1;
      $x += $x;  $x -= $n if $x >= $n;
    }
    if ($y & 1) { $r += $x;  $r -= $n if $r >= $n; }
  } else {
    while ($y > 1) {
      if ($y & 1) { $r = $n-$r;  $r = ($x >= $r) ? $x-$r : $n-$r+$x; }
      $y >>= 1;
      $x = ($x > ($n - $x))  ?  ($x - $n) + $x  :  $x + $x;
    }
    if ($y & 1) { $r = $n-$r;  $r = ($x >= $r) ? $x-$r : $n-$r+$x; }
  }
  $r;
}
sub _addmod {
  my($x, $y, $n) = @_;
  $x %= $n if $x >= $n;
  $y %= $n if $y >= $n;
  if (($n-$x) <= $y) {
    ($x,$y) = ($y,$x) if $y > $x;
    $x -= $n;
  }
  $x + $y;
}

# Note that Perl 5.6.2 with largish 64-bit numbers will break.  As usual.
sub _native_powmod {
  my($n, $power, $m) = @_;
  my $t = 1;
  $n = $n % $m;
  while ($power) {
    $t = ($t * $n) % $m if ($power & 1);
    $power >>= 1;
    $n = ($n * $n) % $m if $power;
  }
  $t;
}

sub _powmod {
  my($n, $power, $m) = @_;
  my $t = 1;

  $n %= $m if $n >= $m;
  if ($m < MPU_HALFWORD) {
    while ($power) {
      $t = ($t * $n) % $m if ($power & 1);
      $power >>= 1;
      $n = ($n * $n) % $m if $power;
    }
  } else {
    while ($power) {
      $t = _mulmod($t, $n, $m) if ($power & 1);
      $power >>= 1;
      $n = _mulmod($n, $n, $m) if $power;
    }
  }
  $t;
}

# Make sure to work around RT71548, Math::BigInt::Lite,
# and use correct lcm semantics.
sub gcd {
  # First see if all inputs are non-bigints  5-10x faster if so.
  if (0 == scalar(grep { ref($_) } @_)) {
    my($x,$y) = (shift || 0, 0);
    while (@_) {
      $y = shift;
      while ($y) {  ($x,$y) = ($y, $x % $y);  }
      $x = -$x if $x < 0;
    }
    return $x;
  }
  my $gcd = Math::BigInt::bgcd( map {
    my $v = (($_ < 2147483647 && !ref($_)) || ref($_) eq 'Math::BigInt') ? $_ : "$_";
    $v;
  } @_ );
  $gcd = _bigint_to_int($gcd) if $gcd->bacmp(BMAX) <= 0;
  return $gcd;
}
sub lcm {
  return 0 unless @_;
  my $lcm = Math::BigInt::blcm( map {
    my $v = (($_ < 2147483647 && !ref($_)) || ref($_) eq 'Math::BigInt') ? $_ : "$_";
    return 0 if $v == 0;
    $v = -$v if $v < 0;
    $v;
  } @_ );
  $lcm = _bigint_to_int($lcm) if $lcm->bacmp(BMAX) <= 0;
  return $lcm;
}
sub gcdext {
  my($x,$y) = @_;
  if ($x == 0) { return (0, (-1,0,1)[($y>=0)+($y>0)], abs($y)); }
  if ($y == 0) { return ((-1,0,1)[($x>=0)+($x>0)], 0, abs($x)); }

  if ($Math::Prime::Util::_GMPfunc{"gcdext"}) {
    my($a,$b,$g) = Math::Prime::Util::GMP::gcdext($x,$y);
    $a = Math::Prime::Util::_reftyped($_[0], $a);
    $b = Math::Prime::Util::_reftyped($_[0], $b);
    $g = Math::Prime::Util::_reftyped($_[0], $g);
    return ($a,$b,$g);
  }

  my($a,$b,$g,$u,$v,$w);
  if (abs($x) < (~0>>1) && abs($y) < (~0>>1)) {
    $x = _bigint_to_int($x) if ref($x) eq 'Math::BigInt';
    $y = _bigint_to_int($y) if ref($y) eq 'Math::BigInt';
    ($a,$b,$g,$u,$v,$w) = (1,0,$x,0,1,$y);
    while ($w != 0) {
      my $r = $g % $w;
      my $q = int(($g-$r)/$w);
      ($a,$b,$g,$u,$v,$w) = ($u,$v,$w,$a-$q*$u,$b-$q*$v,$r);
    }
  } else {
    ($a,$b,$g,$u,$v,$w) = (BONE->copy,BZERO->copy,Math::BigInt->new("$x"),
                           BZERO->copy,BONE->copy,Math::BigInt->new("$y"));
    while ($w != 0) {
      # Using the array bdiv is logical, but is the wrong sign.
      my $r = $g->copy->bmod($w);
      my $q = $g->copy->bsub($r)->bdiv($w);
      ($a,$b,$g,$u,$v,$w) = ($u,$v,$w,$a-$q*$u,$b-$q*$v,$r);
    }
    $a = _bigint_to_int($a) if $a->bacmp(BMAX) <= 0;
    $b = _bigint_to_int($b) if $b->bacmp(BMAX) <= 0;
    $g = _bigint_to_int($g) if $g->bacmp(BMAX) <= 0;
  }
  if ($g < 0) { ($a,$b,$g) = (-$a,-$b,-$g); }
  return ($a,$b,$g);
}

sub chinese {
  return 0 unless scalar @_;
  return $_[0]->[0] % $_[0]->[1] if scalar @_ == 1;
  my($lcm, $sum);

  if ($Math::Prime::Util::_GMPfunc{"chinese"} && $Math::Prime::Util::GMP::VERSION >= 0.42) {
    $sum = Math::Prime::Util::GMP::chinese(@_);
    if (defined $sum) {
      $sum = Math::BigInt->new("$sum");
      $sum = _bigint_to_int($sum) if ref($sum) && $sum->bacmp(BMAX) <= 0;
    }
    return $sum;
  }
  foreach my $aref (sort { $b->[1] <=> $a->[1] } @_) {
    my($ai, $ni) = @$aref;
    $ai = Math::BigInt->new("$ai") if !ref($ai) && (abs($ai) > (~0>>1) || OLD_PERL_VERSION);
    $ni = Math::BigInt->new("$ni") if !ref($ni) && (abs($ni) > (~0>>1) || OLD_PERL_VERSION);
    if (!defined $lcm) {
      ($sum,$lcm) = ($ai % $ni, $ni);
      next;
    }
    # gcdext
    my($u,$v,$g,$s,$t,$w) = (1,0,$lcm,0,1,$ni);
    while ($w != 0) {
      my $r = $g % $w;
      my $q = ref($g)  ?  $g->copy->bsub($r)->bdiv($w)  :  int(($g-$r)/$w);
      ($u,$v,$g,$s,$t,$w) = ($s,$t,$w,$u-$q*$s,$v-$q*$t,$r);
    }
    ($u,$v,$g) = (-$u,-$v,-$g)  if $g < 0;
    return if $g != 1 && ($sum % $g) != ($ai % $g);  # Not co-prime
    $s = -$s if $s < 0;
    $t = -$t if $t < 0;
    # Convert to bigint if necessary.  Performance goes to hell.
    if (!ref($lcm) && ($lcm*$s) > ~0) { $lcm = Math::BigInt->new("$lcm"); }
    if (ref($lcm)) {
      $lcm->bmul("$s");
      my $m1 = Math::BigInt->new("$v")->bmul("$s")->bmod($lcm);
      my $m2 = Math::BigInt->new("$u")->bmul("$t")->bmod($lcm);
      $m1->bmul("$sum")->bmod($lcm);
      $m2->bmul("$ai")->bmod($lcm);
      $sum = $m1->badd($m2)->bmod($lcm);
    } else {
      $lcm *= $s;
      $u += $lcm if $u < 0;
      $v += $lcm if $v < 0;
      my $vs = _mulmod($v,$s,$lcm);
      my $ut = _mulmod($u,$t,$lcm);
      my $m1 = _mulmod($sum,$vs,$lcm);
      my $m2 = _mulmod($ut,$ai % $lcm,$lcm);
      $sum = _addmod($m1, $m2, $lcm);
    }
  }
  $sum = _bigint_to_int($sum) if ref($sum) && $sum->bacmp(BMAX) <= 0;
  $sum;
}

sub _from_128 {
  my($hi, $lo) = @_;
  return 0 unless defined $hi && defined $lo;
  #print "hi $hi lo $lo\n";
  (Math::BigInt->new("$hi") << MPU_MAXBITS) + $lo;
}

sub vecsum {
  return Math::Prime::Util::_reftyped($_[0], @_ ? $_[0] : 0)  if @_ <= 1;

  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::vecsum(@_))
    if $Math::Prime::Util::_GMPfunc{"vecsum"};
  my $sum = 0;
  my $neglim = -(INTMAX >> 1) - 1;
  foreach my $v (@_) {
    $sum += $v;
    if ($sum > (INTMAX-250) || $sum < $neglim) {
      $sum = BZERO->copy;
      $sum->badd("$_") for @_;
      return $sum;
    }
  }
  $sum;
}

sub vecprod {
  return 1 unless @_;
  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::vecprod(@_))
    if $Math::Prime::Util::_GMPfunc{"vecprod"};
  # Product tree:
  my $prod = _product(0, $#_, [map { Math::BigInt->new("$_") } @_]);
  # Linear:
  # my $prod = BONE->copy;  $prod *= "$_" for @_;
  $prod = _bigint_to_int($prod) if $prod->bacmp(BMAX) <= 0 && $prod->bcmp(-(BMAX>>1)) > 0;
  $prod;
}

sub vecmin {
  return unless @_;
  my $min = shift;
  for (@_) { $min = $_ if $_ < $min; }
  $min;
}
sub vecmax {
  return unless @_;
  my $max = shift;
  for (@_) { $max = $_ if $_ > $max; }
  $max;
}

sub vecextract {
  my($aref, $mask) = @_;

  return @$aref[@$mask] if ref($mask) eq 'ARRAY';

  # This is concise but very slow.
  # map { $aref->[$_] }  grep { $mask & (1 << $_) }  0 .. $#$aref;

  my($i, @v) = (0);
  while ($mask) {
    push @v, $i if $mask & 1;
    $mask >>= 1;
    $i++;
  }
  @$aref[@v];
}

sub sumdigits {
  my($n,$base) = @_;
  my $sum = 0;
  $base =  2 if !defined $base && $n =~ s/^0b//;
  $base = 16 if !defined $base && $n =~ s/^0x//;
  if (!defined $base || $base == 10) {
    $n =~ tr/0123456789//cd;
    $sum += $_ for (split(//,$n));
  } else {
    croak "sumdigits: invalid base $base" if $base < 2;
    my $cmap = substr("0123456789abcdefghijklmnopqrstuvwxyz",0,$base);
    for my $c (split(//,lc($n))) {
      my $p = index($cmap,$c);
      $sum += $p if $p > 0;
    }
  }
  $sum;
}

sub invmod {
  my($a,$n) = @_;
  return if $n == 0 || $a == 0;
  return 0 if $n == 1;
  $n = -$n if $n < 0;  # Pari semantics
  if ($n > ~0) {
    my $invmod = Math::BigInt->new("$a")->bmodinv("$n");
    return if !defined $invmod || $invmod->is_nan;
    $invmod = _bigint_to_int($invmod) if $invmod->bacmp(BMAX) <= 0;
    return $invmod;
  }
  my($t,$nt,$r,$nr) = (0, 1, $n, $a % $n);
  while ($nr != 0) {
    # Use mod before divide to force correct behavior with high bit set
    my $quot = int( ($r-($r % $nr))/$nr );
    ($nt,$t) = ($t-$quot*$nt,$nt);
    ($nr,$r) = ($r-$quot*$nr,$nr);
  }
  return if $r > 1;
  $t += $n if $t < 0;
  $t;
}

sub _verify_sqrtmod {
  my($r,$a,$n) = @_;
  if (ref($r)) {
    return if $r->copy->bmul($r)->bmod($n)->bcmp($a);
    $r = _bigint_to_int($r) if $r->bacmp(BMAX) <= 0;
  } else {
    return unless (($r*$r) % $n) == $a;
  }
  $r = $n-$r if $n-$r < $r;
  $r;
}

sub sqrtmod {
  my($a,$n) = @_;
  return if $n == 0;
  if ($n <= 2 || $a <= 1) {
    $a %= $n;
    return ((($a*$a) % $n) == $a) ? $a : undef;
  }

  if ($n < 10000000) {
    # Horrible trial search
    $a = _bigint_to_int($a);
    $n = _bigint_to_int($n);
    $a %= $n;
    return 1 if $a == 1;
    my $lim = ($n+1) >> 1;
    for my $r (2 .. $lim) {
      return $r if (($r*$r) % $n) == $a;
    }
    undef;
  }

  $a = Math::BigInt->new("$a") unless ref($a) eq 'Math::BigInt';
  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
  $a->bmod($n);
  my $r;

  if (($n % 4) == 3) {
    $r = $a->copy->bmodpow(($n+1)>>2, $n);
    return _verify_sqrtmod($r, $a, $n);
  }
  if (($n % 8) == 5) {
    my $q = $a->copy->bmodpow(($n-1)>>2, $n);
    if ($q->is_one) {
      $r = $a->copy->bmodpow(($n+3)>>3, $n);
    } else {
      my $v = $a->copy->bmul(4)->bmodpow(($n-5)>>3, $n);
      $r = $a->copy->bmul(2)->bmul($v)->bmod($n);
    }
    return _verify_sqrtmod($r, $a, $n);
  }

  return if $n->is_odd && !$a->copy->bmodpow(($n-1)>>1,$n)->is_one();

  # Horrible trial search.  Need to use Tonelli-Shanks here.
  $r = Math::BigInt->new(2);
  my $lim = int( ($n+1) / 2 );
  while ($r < $lim) {
    return $r if $r->copy->bmul($r)->bmod($n) == $a;
    $r++;
  }
  undef;
}

sub addmod {
  my($a, $b, $n) = @_;
  return 0 if $n <= 1;
  return _addmod($a,$b,$n) if $n < INTMAX && $a>=0 && $a<INTMAX && $b>=0 && $b<INTMAX;
  my $ret = Math::BigInt->new("$a")->badd("$b")->bmod("$n");
  $ret = _bigint_to_int($ret) if $ret->bacmp(BMAX) <= 0;
  $ret;
}

sub mulmod {
  my($a, $b, $n) = @_;
  return 0 if $n <= 1;
  return _mulmod($a,$b,$n) if $n < INTMAX && $a>0 && $a<INTMAX && $b>0 && $b<INTMAX;
  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::mulmod($a,$b,$n))
    if $Math::Prime::Util::_GMPfunc{"mulmod"};
  my $ret = Math::BigInt->new("$a")->bmod("$n")->bmul("$b")->bmod("$n");
  $ret = _bigint_to_int($ret) if $ret->bacmp(BMAX) <= 0;
  $ret;
}
sub divmod {
  my($a, $b, $n) = @_;
  return 0 if $n <= 1;
  my $ret = Math::BigInt->new("$b")->bmodinv("$n")->bmul("$a")->bmod("$n");
  if ($ret->is_nan) {
    $ret = undef;
  } else {
    $ret = _bigint_to_int($ret) if $ret->bacmp(BMAX) <= 0;
  }
  $ret;
}
sub powmod {
  my($a, $b, $n) = @_;
  return 0 if $n <= 1;
  if ($Math::Prime::Util::_GMPfunc{"powmod"}) {
    my $r = Math::Prime::Util::GMP::powmod($a,$b,$n);
    return (defined $r) ? Math::Prime::Util::_reftyped($_[0], $r) : undef;
  }
  my $ret = Math::BigInt->new("$a")->bmod("$n")->bmodpow("$b","$n");
  if ($ret->is_nan) {
    $ret = undef;
  } else {
    $ret = _bigint_to_int($ret) if $ret->bacmp(BMAX) <= 0;
  }
  $ret;
}

# no validation, x is allowed to be negative, y must be >= 0
sub _gcd_ui {
  my($x, $y) = @_;
  if ($y < $x) { ($x, $y) = ($y, $x); }
  elsif ($x < 0) { $x = -$x; }
  while ($y > 0) {
    ($x, $y) = ($y, $x % $y);
  }
  $x;
}

sub is_power {
  my ($n, $a, $refp) = @_;
  croak("is_power third argument not a scalar reference") if defined($refp) && !ref($refp);
  _validate_integer($n);
  return 0 if abs($n) <= 3 && !$a;

  if ($Math::Prime::Util::_GMPfunc{"is_power"} &&
      ($Math::Prime::Util::GMP::VERSION >= 0.42 ||
       ($Math::Prime::Util::GMP::VERSION >= 0.28 && $n > 0))) {
    $a = 0 unless defined $a;
    my $k = Math::Prime::Util::GMP::is_power($n,$a);
    return 0 unless $k > 0;
    if (defined $refp) {
      $a = $k unless $a;
      my $isneg = ($n < 0);
      $n =~ s/^-// if $isneg;
      $$refp = Math::Prime::Util::rootint($n, $a);
      $$refp = Math::Prime::Util::_reftyped($_[0], $$refp) if $$refp > INTMAX;
      $$refp = -$$refp if $isneg;
    }
    return $k;
  }

  if (defined $a && $a != 0) {
    return 1 if $a == 1;                  # Everything is a 1st power
    return 0 if $n < 0 && $a % 2 == 0;    # Negative n never an even power
    if ($a == 2) {
      if (_is_perfect_square($n)) {
        $$refp = int(sqrt($n)) if defined $refp;
        return 1;
      }
    } else {
      $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
      my $root = $n->copy->babs->broot($a)->bfloor;
      $root->bneg if $n->is_neg;
      if ($root->copy->bpow($a) == $n) {
        $$refp = $root if defined $refp;
        return 1;
      }
    }
  } else {
    $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
    if ($n < 0) {
      my $absn = $n->copy->babs;
      my $root = is_power($absn, 0, $refp);
      return 0 unless $root;
      if ($root % 2 == 0) {
        my $power = valuation($root, 2);
        $root >>= $power;
        return 0 if $root == 1;
        $power = BTWO->copy->bpow($power);
        $$refp = $$refp ** $power if defined $refp;
      }
      $$refp = -$$refp if defined $refp;
      return $root;
    }
    my $e = 2;
    while (1) {
      my $root = $n->copy()->broot($e)->bfloor;
      last if $root->is_one();
      if ($root->copy->bpow($e) == $n) {
        my $next = is_power($root, 0, $refp);
        $$refp = $root if !$next && defined $refp;
        $e *= $next if $next != 0;
        return $e;
      }
      $e = next_prime($e);
    }
  }
  0;
}

sub is_square {
  my($n) = @_;
  return 0 if $n < 0;
  #is_power($n,2);
  _validate_integer($n);
  _is_perfect_square($n);
}

sub is_prime_power {
  my ($n, $refp) = @_;
  croak("is_prime_power second argument not a scalar reference") if defined($refp) && !ref($refp);
  return 0 if $n <= 1;

  if (Math::Prime::Util::is_prime($n)) { $$refp = $n if defined $refp; return 1; }
  my $r;
  my $k = Math::Prime::Util::is_power($n,0,\$r);
  if ($k) {
    $r = _bigint_to_int($r) if ref($r) && $r->bacmp(BMAX) <= 0;
    return 0 unless Math::Prime::Util::is_prime($r);
    $$refp = $r if defined $refp;
  }
  $k;
}

sub is_polygonal {
  my ($n, $k, $refp) = @_;
  croak("is_polygonal third argument not a scalar reference") if defined($refp) && !ref($refp);
  croak("is_polygonal: k must be >= 3") if $k < 3;
  return 0 if $n <= 0;
  if ($n == 1) { $$refp = 1 if defined $refp; return 1; }

  if ($Math::Prime::Util::_GMPfunc{"polygonal_nth"}) {
    my $nth = Math::Prime::Util::GMP::polygonal_nth($n, $k);
    return 0 unless $nth;
    $nth = Math::Prime::Util::_reftyped($_[0], $nth);
    $$refp = $nth if defined $refp;
    return 1;
  }

  my($D,$R);
  if ($k == 4) {
    return 0 unless _is_perfect_square($n);
    $$refp = sqrtint($n) if defined $refp;
    return 1;
  }
  if ($n <= MPU_HALFWORD && $k <= MPU_HALFWORD) {
    $D = ($k==3) ? 1+($n<<3) : (8*$k-16)*$n + ($k-4)*($k-4);
    return 0 unless _is_perfect_square($D);
    $D = $k-4 + Math::Prime::Util::sqrtint($D);
    $R = 2*$k-4;
  } else {
    if ($k == 3) {
      $D = vecsum(1, vecprod($n, 8));
    } else {
      $D = vecsum(vecprod($n, vecprod(8, $k) - 16),  vecprod($k-4,$k-4));;
    }
    return 0 unless _is_perfect_square($D);
    $D = vecsum( sqrtint($D), $k-4 );
    $R = vecprod(2, $k) - 4;
  }
  return 0 if ($D % $R) != 0;
  $$refp = $D / $R if defined $refp;
  1;
}

sub valuation {
  my($n, $k) = @_;
  $n = -$n if defined $n && $n < 0;
  _validate_num($n) || _validate_positive_integer($n);
  return 0 if $n < 2 || $k < 2;
  my $v = 0;
  if ($k == 2) { # Accelerate power of 2
    if (ref($n) eq 'Math::BigInt') {   # This can pay off for big inputs
      return 0 unless $n->is_even;
      my $s = $n->as_bin;              # We could do same for k=10
      return length($s) - rindex($s,'1') - 1;
    }
    while (!($n & 0xFFFF) ) {  $n >>=16;  $v +=16;  }
    while (!($n & 0x000F) ) {  $n >>= 4;  $v += 4;  }
  }
  while ( !($n % $k) ) {
    $n /= $k;
    $v++;
  }
  $v;
}

sub hammingweight {
  my $n = shift;
  return 0 + (Math::BigInt->new("$n")->as_bin() =~ tr/1//);
}

my @_digitmap = (0..9, 'a'..'z');
my %_mapdigit = map { $_digitmap[$_] => $_ } 0 .. $#_digitmap;
sub _splitdigits {
  my($n, $base, $len) = @_;    # n is num or bigint, base is in range
  my @d;
  if ($base == 10) {
    @d = split(//,"$n");
  } elsif ($base == 2) {
    @d = split(//,substr(Math::BigInt->new("$n")->as_bin,2));
  } elsif ($base == 16) {
    @d = map { $_mapdigit{$_} } split(//,substr(Math::BigInt->new("$n")->as_hex,2));
  } else {
    while ($n >= 1) {
      my $rem = $n % $base;
      unshift @d, $rem;
      $n = ($n-$rem)/$base;    # Always an exact division
    }
  }
  if ($len >= 0 && $len != scalar(@d)) {
    while (@d < $len) { unshift @d, 0; }
    while (@d > $len) { shift @d; }
  }
  @d;
}

sub todigits {
  my($n,$base,$len) = @_;
  $base = 10 unless defined $base;
  $len = -1 unless defined $len;
  die "Invalid base: $base" if $base < 2;
  return if $n == 0;
  $n = -$n if $n < 0;
  _validate_num($n) || _validate_positive_integer($n);
  _splitdigits($n, $base, $len);
}

sub todigitstring {
  my($n,$base,$len) = @_;
  $base = 10 unless defined $base;
  $len = -1 unless defined $len;
  $n =~ s/^-//;
  return substr(Math::BigInt->new("$n")->as_bin,2) if $base ==  2 && $len < 0;
  return substr(Math::BigInt->new("$n")->as_oct,1) if $base ==  8 && $len < 0;
  return substr(Math::BigInt->new("$n")->as_hex,2) if $base == 16 && $len < 0;
  my @d = ($n == 0) ? () : _splitdigits($n, $base, $len);
  return join("", @d) if $base <= 10;
  die "Invalid base for string: $base" if $base > 36;
  join("", map { $_digitmap[$_] } @d);
}

sub fromdigits {
  my($r, $base) = @_;
  $base = 10 unless defined $base;
  return $r if $base == 10 && ref($r) =~ /^Math::/;
  my $n;
  if (ref($r) && ref($r) !~ /^Math::/) {
    croak "fromdigits first argument must be a string or array reference"
      unless ref($r) eq 'ARRAY';
    ($n,$base) = (BZERO->copy, BZERO + $base);
    for my $d (@$r) {
      $n = $n * $base + $d;
    }
  } elsif ($base == 2) {
    $n = Math::BigInt->from_bin("0b$r");
  } elsif ($base == 8) {
    $n = Math::BigInt->from_oct("0$r");
  } elsif ($base == 16) {
    $n = Math::BigInt->from_hex("0x$r");
  } else {
    $r =~ s/^0*//;
    ($n,$base) = (BZERO->copy, BZERO + $base);
    #for my $d (map { $_mapdigit{$_} } split(//,$r)) {
    #  croak "Invalid digit for base $base" unless defined $d && $d < $base;
    #  $n = $n * $base + $d;
    #}
    for my $c (split(//, lc($r))) {
      $n->bmul($base);
      if ($c ne '0') {
        my $d = index("0123456789abcdefghijklmnopqrstuvwxyz", $c);
        croak "Invalid digit for base $base" unless $d >= 0;
        $n->badd($d);
      }
    }
  }
  $n = _bigint_to_int($n) if $n->bacmp(BMAX) <= 0;
  $n;
}

sub sqrtint {
  my($n) = @_;
  my $sqrt = Math::BigInt->new("$n")->bsqrt;
  return Math::Prime::Util::_reftyped($_[0], "$sqrt");
}

sub rootint {
  my ($n, $k, $refp) = @_;
  croak "rootint: k must be > 0" unless $k > 0;
  # Math::BigInt returns NaN for any root of a negative n.
  my $root = Math::BigInt->new("$n")->babs->broot("$k");
  if (defined $refp) {
    croak("logint third argument not a scalar reference") unless ref($refp);
    $$refp = $root->copy->bpow($k);
  }
  return Math::Prime::Util::_reftyped($_[0], "$root");
}

sub logint {
  my ($n, $b, $refp) = @_;
  croak("logint third argument not a scalar reference") if defined($refp) && !ref($refp);

  if ($Math::Prime::Util::_GMPfunc{"logint"}) {
    my $e = Math::Prime::Util::GMP::logint($n, $b);
    if (defined $refp) {
      my $r = Math::Prime::Util::GMP::powmod($b, $e, $n);
      $r = $n if $r == 0;
      $$refp = Math::Prime::Util::_reftyped($_[0], $r);
    }
    return Math::Prime::Util::_reftyped($_[0], $e);
  }

  croak "logint: n must be > 0" unless $n > 0;
  croak "logint: missing base" unless defined $b;
  if ($b == 10) {
    my $e = length($n)-1;
    $$refp = Math::BigInt->new("1" . "0"x$e) if defined $refp;
    return $e;
  }
  if ($b == 2) {
    my $e = length(Math::BigInt->new("$n")->as_bin)-2-1;
    $$refp = Math::BigInt->from_bin("1" . "0"x$e) if defined $refp;
    return $e;
  }
  croak "logint: base must be > 1" unless $b > 1;

  my $e = Math::BigInt->new("$n")->blog("$b");
  $$refp = Math::BigInt->new("$b")->bpow($e) if defined $refp;
  return Math::Prime::Util::_reftyped($_[0], "$e");
}

# Seidel (Luschny), core using Trizen's simplications from Math::BigNum.
# http://oeis.org/wiki/User:Peter_Luschny/ComputationAndAsymptoticsOfBernoulliNumbers#Bernoulli_numbers__after_Seidel
sub _bernoulli_seidel {
  my($n) = @_;
  return (1,1) if $n == 0;
  return (0,1) if $n > 1 && $n % 2;

  my $oacc = Math::BigInt->accuracy();  Math::BigInt->accuracy(undef);
  my @D = (BZERO->copy, BONE->copy, map { BZERO->copy } 1 .. ($n>>1)-1);
  my ($h, $w) = (1, 1);

  foreach my $i (0 .. $n-1) {
    if ($w ^= 1) {
      $D[$_]->badd($D[$_-1]) for 1 .. $h-1;
    } else {
      $w = $h++;
      $D[$w]->badd($D[$w+1]) while --$w;
    }
  }
  my $num = $D[$h-1];
  my $den = BONE->copy->blsft($n+1)->bsub(BTWO);
  my $gcd = Math::BigInt::bgcd($num, $den);
  $num /= $gcd;
  $den /= $gcd;
  $num->bneg() if ($n % 4) == 0;
  Math::BigInt->accuracy($oacc);
  ($num,$den);
}

sub bernfrac {
  my $n = shift;
  return (BONE,BONE) if $n == 0;
  return (BONE,BTWO) if $n == 1;    # We're choosing 1/2 instead of -1/2
  return (BZERO,BONE) if $n < 0 || $n & 1;

  # We should have used one of the GMP functions before coming here.

  _bernoulli_seidel($n);
}

sub stirling {
  my($n, $m, $type) = @_;
  return 1 if $m == $n;
  return 0 if $n == 0 || $m == 0 || $m > $n;
  $type = 1 unless defined $type;
  croak "stirling type must be 1, 2, or 3" unless $type == 1 || $type == 2 || $type == 3;
  if ($m == 1) {
    return 1 if $type == 2;
    return factorial($n) if $type == 3;
    return factorial($n-1) if $n&1;
    return vecprod(-1, factorial($n-1));
  }
  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::stirling($n,$m,$type))
    if $Math::Prime::Util::_GMPfunc{"stirling"};
  # Go through vecsum with quoted negatives to make sure we don't overflow.
  my $s;
  if ($type == 3) {
    $s = Math::Prime::Util::vecprod( Math::Prime::Util::binomial($n,$m), Math::Prime::Util::binomial($n-1,$m-1), Math::Prime::Util::factorial($n-$m) );
  } elsif ($type == 2) {
    my @terms;
    for my $j (1 .. $m) {
      my $t = Math::Prime::Util::vecprod(
                Math::BigInt->new($j) ** $n,
                Math::Prime::Util::binomial($m,$j)
              );
      push @terms, (($m-$j) & 1)  ?  "-$t"  :  $t;
    }
    $s = Math::Prime::Util::vecsum(@terms) / factorial($m);
  } else {
    my @terms;
    for my $k (1 .. $n-$m) {
      my $t = Math::Prime::Util::vecprod(
        Math::Prime::Util::binomial($k + $n - 1, $k + $n - $m),
        Math::Prime::Util::binomial(2 * $n - $m, $n - $k - $m),
        Math::Prime::Util::stirling($k - $m + $n, $k, 2),
      );
      push @terms, ($k & 1)  ?  "-$t"  :  $t;
    }
    $s = Math::Prime::Util::vecsum(@terms);
  }
  $s;
}

sub _harmonic_split { # From Fredrik Johansson
  my($a,$b) = @_;
  return (BONE, $a) if $b - $a == BONE;
  return ($a+$a+BONE, $a*$a+$a) if $b - $a == BTWO;   # Cut down recursion
  my $m = $a->copy->badd($b)->brsft(BONE);
  my ($p,$q) = _harmonic_split($a, $m);
  my ($r,$s) = _harmonic_split($m, $b);
  ($p*$s+$q*$r, $q*$s);
}

sub harmfrac {
  my($n) = @_;
  return (BZERO,BONE) if $n <= 0;
  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
  my($p,$q) = _harmonic_split($n-$n+1, $n+1);
  my $gcd = Math::BigInt::bgcd($p,$q);
  ( scalar $p->bdiv($gcd), scalar $q->bdiv($gcd) );
}

sub harmreal {
  my($n, $precision) = @_;

  do { require Math::BigFloat; Math::BigFloat->import(); } unless defined $Math::BigFloat::VERSION;
  return Math::BigFloat->bzero if $n <= 0;

  # Use asymptotic formula for larger $n if possible.  Saves lots of time if
  # the default Calc backend is being used.
  {
    my $sprec = $precision;
    $sprec = Math::BigFloat->precision unless defined $sprec;
    $sprec = 40 unless defined $sprec;
    if ( ($sprec <= 23 && $n >    54) ||
         ($sprec <= 30 && $n >   348) ||
         ($sprec <= 40 && $n >  2002) ||
         ($sprec <= 50 && $n > 12644) ) {
      $n = Math::BigFloat->new($n, $sprec+5);
      my($n2, $one, $h) = ($n*$n, Math::BigFloat->bone, Math::BigFloat->bzero);
      my $nt = $n2;
      my $eps = Math::BigFloat->new(10)->bpow(-$sprec-4);
      foreach my $d (-12, 120, -252, 240, -132, 32760, -12, 8160, -14364, 6600, -276, 65520, -12) { # OEIS A006593
        my $term = $one/($d * $nt);
        last if $term->bacmp($eps) < 0;
        $h += $term;
        $nt *= $n2;
      }
      $h->badd(scalar $one->copy->bdiv(2*$n));
      $h->badd(_Euler($sprec));
      $h->badd($n->copy->blog);
      $h->round($sprec);
      return $h;
    }
  }

  my($num,$den) = Math::Prime::Util::harmfrac($n);
  # Note, with Calc backend this can be very, very slow
  scalar Math::BigFloat->new($num)->bdiv($den, $precision);
}

sub is_pseudoprime {
  my($n, @bases) = @_;
  return 0 if int($n) < 0;
  _validate_positive_integer($n);
  croak("No bases given to is_pseudoprime") unless scalar(@bases) > 0;
  return 0+($n >= 2) if $n < 4;

  foreach my $base (@bases) {
    croak "Base $base is invalid" if $base < 2;
    $base = $base % $n if $base >= $n;
    if ($base > 1 && $base != $n-1) {
      my $x = (ref($n) eq 'Math::BigInt')
        ? $n->copy->bzero->badd($base)->bmodpow($n-1,$n)->is_one
        : _powmod($base, $n-1, $n);
      return 0 unless $x == 1;
    }
  }
  1;
}

sub is_euler_pseudoprime {
  my($n, @bases) = @_;
  return 0 if int($n) < 0;
  _validate_positive_integer($n);
  croak("No bases given to is_euler_pseudoprime") unless scalar(@bases) > 0;
  return 0+($n >= 2) if $n < 4;

  foreach my $base (@bases) {
    croak "Base $base is invalid" if $base < 2;
    $base = $base % $n if $base >= $n;
    if ($base > 1 && $base != $n-1) {
      my $j = kronecker($base, $n);
      return 0 if $j == 0;
      $j = ($j > 0) ? 1 : $n-1;
      my $x = (ref($n) eq 'Math::BigInt')
        ? $n->copy->bzero->badd($base)->bmodpow(($n-1)/2,$n)
        : _powmod($base, ($n-1)>>1, $n);
      return 0 unless $x == $j;
    }
  }
  1;
}

sub is_euler_plumb_pseudoprime {
  my($n) = @_;
  return 0 if int($n) < 0;
  _validate_positive_integer($n);
  return 0+($n >= 2) if $n < 4;
  return 0 if ($n % 2) == 0;
  my $nmod8 = $n % 8;
  my $exp = 1 + ($nmod8 == 1);
  my $ap = Math::Prime::Util::powmod(2, ($n-1) >> $exp, $n);
  if ($ap ==    1) { return ($nmod8 == 1 || $nmod8 == 7); }
  if ($ap == $n-1) { return ($nmod8 == 1 || $nmod8 == 3 || $nmod8 == 5); }
  0;
}

sub _miller_rabin_2 {
  my($n, $nm1, $s, $d) = @_;

  if ( ref($n) eq 'Math::BigInt' ) {

    if (!defined $nm1) {
      $nm1 = $n->copy->bdec();
      $s = 0;
      $d = $nm1->copy;
      do {
        $s++;
        $d->brsft(BONE);
      } while $d->is_even;
    }
    my $x = BTWO->copy->bmodpow($d,$n);
    return 1 if $x->is_one || $x->bcmp($nm1) == 0;
    foreach my $r (1 .. $s-1) {
      $x->bmul($x)->bmod($n);
      last if $x->is_one;
      return 1 if $x->bcmp($nm1) == 0;
    }

  } else {

    if (!defined $nm1) {
      $nm1 = $n-1;
      $s = 0;
      $d = $nm1;
      while ( ($d & 1) == 0 ) {
        $s++;
        $d >>= 1;
      }
    }

    if ($n < MPU_HALFWORD) {
      my $x = _native_powmod(2, $d, $n);
      return 1 if $x == 1 || $x == $nm1;
      foreach my $r (1 .. $s-1) {
        $x = ($x*$x) % $n;
        last if $x == 1;
        return 1 if $x == $n-1;
      }
    } else {
      my $x = _powmod(2, $d, $n);
      return 1 if $x == 1 || $x == $nm1;
      foreach my $r (1 .. $s-1) {
        $x = ($x < MPU_HALFWORD) ? ($x*$x) % $n : _mulmod($x, $x, $n);
        last if $x == 1;
        return 1 if $x == $n-1;
      }
    }
  }
  0;
}

sub is_strong_pseudoprime {
  my($n, @bases) = @_;
  return 0 if int($n) < 0;
  _validate_positive_integer($n);
  croak("No bases given to is_strong_pseudoprime") unless scalar(@bases) > 0;

  return 0+($n >= 2) if $n < 4;
  return 0 if ($n % 2) == 0;

  if ($bases[0] == 2) {
    return 0 unless _miller_rabin_2($n);
    shift @bases;
    return 1 unless @bases;
  }

  my @newbases;
  for my $base (@bases) {
    croak "Base $base is invalid" if $base < 2;
    $base %= $n if $base >= $n;
    return 0 if $base == 0 || ($base == $n-1 && ($base % 2) == 1);
    push @newbases, $base;
  }
  @bases = @newbases;

  if ( ref($n) eq 'Math::BigInt' ) {

    my $nminus1 = $n->copy->bdec();
    my $s = 0;
    my $d = $nminus1->copy;
    do {  # n is > 3 and odd, so n-1 must be even
      $s++;
      $d->brsft(BONE);
    } while $d->is_even;
    # Different way of doing the above.  Fewer function calls, slower on ave.
    #my $dbin = $nminus1->as_bin;
    #my $last1 = rindex($dbin, '1');
    #my $s = length($dbin)-2-$last1+1;
    #my $d = $nminus1->copy->brsft($s);

    foreach my $ma (@bases) {
      my $x = $n->copy->bzero->badd($ma)->bmodpow($d,$n);
      next if $x->is_one || $x->bcmp($nminus1) == 0;
      foreach my $r (1 .. $s-1) {
        $x->bmul($x); $x->bmod($n);
        return 0 if $x->is_one;
        do { $ma = 0; last; } if $x->bcmp($nminus1) == 0;
      }
      return 0 if $ma != 0;
    }

  } else {

   my $s = 0;
   my $d = $n - 1;
   while ( ($d & 1) == 0 ) {
     $s++;
     $d >>= 1;
   }

   if ($n < MPU_HALFWORD) {
    foreach my $ma (@bases) {
      my $x = _native_powmod($ma, $d, $n);
      next if ($x == 1) || ($x == ($n-1));
      foreach my $r (1 .. $s-1) {
        $x = ($x*$x) % $n;
        return 0 if $x == 1;
        last if $x == $n-1;
      }
      return 0 if $x != $n-1;
    }
   } else {
    foreach my $ma (@bases) {
      my $x = _powmod($ma, $d, $n);
      next if ($x == 1) || ($x == ($n-1));

      foreach my $r (1 .. $s-1) {
        $x = ($x < MPU_HALFWORD) ? ($x*$x) % $n : _mulmod($x, $x, $n);
        return 0 if $x == 1;
        last if $x == $n-1;
      }
      return 0 if $x != $n-1;
    }
   }

  }
  1;
}


# Calculate Kronecker symbol (a|b).  Cohen Algorithm 1.4.10.
# Extension of the Jacobi symbol, itself an extension of the Legendre symbol.
sub kronecker {
  my($a, $b) = @_;
  return (abs($a) == 1) ? 1 : 0  if $b == 0;
  my $k = 1;
  if ($b % 2 == 0) {
    return 0 if $a % 2 == 0;
    my $v = 0;
    do { $v++; $b /= 2; } while $b % 2 == 0;
    $k = -$k if $v % 2 == 1 && ($a % 8 == 3 || $a % 8 == 5);
  }
  if ($b < 0) {
    $b = -$b;
    $k = -$k if $a < 0;
  }
  if ($a < 0) { $a = -$a; $k = -$k if $b % 4 == 3; }
  $b = _bigint_to_int($b) if ref($b) eq 'Math::BigInt' && $b <= BMAX;
  $a = _bigint_to_int($a) if ref($a) eq 'Math::BigInt' && $a <= BMAX;
  # Now:  b > 0, b odd, a >= 0
  while ($a != 0) {
    if ($a % 2 == 0) {
      my $v = 0;
      do { $v++; $a /= 2; } while $a % 2 == 0;
      $k = -$k if $v % 2 == 1 && ($b % 8 == 3 || $b % 8 == 5);
    }
    $k = -$k if $a % 4 == 3 && $b % 4 == 3;
    ($a, $b) = ($b % $a, $a);
    # If a,b are bigints and now small enough, finish as native.
    if (   ref($a) eq 'Math::BigInt' && $a <= BMAX
        && ref($b) eq 'Math::BigInt' && $b <= BMAX) {
      return $k * kronecker(_bigint_to_int($a),_bigint_to_int($b));
    }
  }
  return ($b == 1) ? $k : 0;
}

sub _binomialu {
  my($r, $n, $k) = (1, @_);
  return ($k == $n) ? 1 : 0 if $k >= $n;
  $k = $n - $k if $k > ($n >> 1);
  foreach my $d (1 .. $k) {
    if ($r >= int(~0/$n)) {
      my($g, $nr, $dr);
      $g = _gcd_ui($n, $d);   $nr = int($n/$g);   $dr = int($d/$g);
      $g = _gcd_ui($r, $dr);  $r  = int($r/$g);   $dr = int($dr/$g);
      return 0 if $r >= int(~0/$nr);
      $r *= $nr;
      $r = int($r/$dr);
    } else {
      $r *= $n;
      $r = int($r/$d);
    }
    $n--;
  }
  $r;
}

sub binomial {
  my($n, $k) = @_;

  # 1. Try GMP
  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::binomial($n,$k))
    if $Math::Prime::Util::_GMPfunc{"binomial"};

  # 2. Exit early for known 0 cases, and adjust k to be positive.
  if ($n >= 0) {  return 0 if $k < 0 || $k > $n;  }
  else         {  return 0 if $k < 0 && $k > $n;  }
  $k = $n - $k if $k < 0;

  # 3. Try to do in integer Perl
  my $r;
  if ($n >= 0) {
    $r = _binomialu($n, $k);
    return $r  if $r > 0;
  } else {
    $r = _binomialu(-$n+$k-1, $k);
    return $r   if $r > 0 && !($k & 1);
    return -$r  if $r > 0 && $r <= (~0>>1);
  }

  # 4. Overflow.  Solve using Math::BigInt
  return 1 if $k == 0;        # Work around bug in old
  return $n if $k == $n-1;    # Math::BigInt (fixed in 1.90)
  if ($n >= 0) {
    $r = Math::BigInt->new(''.$n)->bnok($k);
    $r = _bigint_to_int($r) if $r->bacmp(BMAX) <= 0;
  } else { # Math::BigInt is incorrect for negative n
    $r = Math::BigInt->new(''.(-$n+$k-1))->bnok($k);
    if ($k & 1) {
      $r->bneg;
      $r = _bigint_to_int($r) if $r->bacmp(''.(~0>>1)) <= 0;
    } else {
      $r = _bigint_to_int($r) if $r->bacmp(BMAX) <= 0;
    }
  }
  $r;
}

sub _product {
  my($a, $b, $r) = @_;
  if ($b <= $a) {
    $r->[$a];
  } elsif ($b == $a+1) {
    $r->[$a] -> bmul( $r->[$b] );
  } elsif ($b == $a+2) {
    $r->[$a] -> bmul( $r->[$a+1] ) -> bmul( $r->[$a+2] );
  } else {
    my $c = $a + (($b-$a+1)>>1);
    _product($a, $c-1, $r);
    _product($c, $b, $r);
    $r->[$a] -> bmul( $r->[$c] );
  }
}

sub factorial {
  my($n) = @_;
  return (1,1,2,6,24,120,720,5040,40320,362880,3628800,39916800,479001600)[$n] if $n <= 12;
  return Math::GMP::bfac($n) if ref($n) eq 'Math::GMP';
  do { my $r = Math::GMPz->new(); Math::GMPz::Rmpz_fac_ui($r,$n); return $r; }
    if ref($n) eq 'Math::GMPz';
  if (Math::BigInt->config()->{lib} !~ /GMP|Pari/) {
    # It's not a GMP or GMPz object, and we have a slow bigint library.
    my $r;
    if (defined $Math::GMPz::VERSION) {
      $r = Math::GMPz->new(); Math::GMPz::Rmpz_fac_ui($r,$n);
    } elsif (defined $Math::GMP::VERSION) {
      $r = Math::GMP::bfac($n);
    } elsif (defined &Math::Prime::Util::GMP::factorial && Math::Prime::Util::prime_get_config()->{'gmp'}) {
      $r = Math::Prime::Util::GMP::factorial($n);
    }
    return Math::Prime::Util::_reftyped($_[0], $r)    if defined $r;
  }
  my $r = Math::BigInt->new($n)->bfac();
  $r = _bigint_to_int($r) if $r->bacmp(BMAX) <= 0;
  $r;
}

sub factorialmod {
  my($n,$m) = @_;

  return Math::Prime::Util::GMP::factorialmod($n,$m)
    if $Math::Prime::Util::_GMPfunc{"factorialmod"};

  return 0 if $n >= $m || $m == 1;

  if ($n > 10) {
    my($s,$t,$e) = (1);
    Math::Prime::Util::forprimes( sub {
      ($t,$e) = ($n,0);
      while ($t > 0) {
        $t = int($t/$_);
        $e += $t;
      }
      $s = Math::Prime::Util::mulmod($s, Math::Prime::Util::powmod($_,$e,$m), $m);
    }, 2, $n >> 1);
    Math::Prime::Util::forprimes( sub {
      $s = Math::Prime::Util::mulmod($s, $_, $m);
    }, ($n >> 1)+1, $n);
    return $s;
  }

  return factorial($n) % $m;
}

sub _is_perfect_square {
  my($n) = @_;
  return (1,1,0,0,1)[$n] if $n <= 4;

  if (ref($n) eq 'Math::BigInt') {
    my $mc = _bigint_to_int($n & 31);
    if ($mc==0||$mc==1||$mc==4||$mc==9||$mc==16||$mc==17||$mc==25) {
      my $sq = $n->copy->bsqrt->bfloor;
      $sq->bmul($sq);
      return 1 if $sq == $n;
    }
  } else {
    my $mc = $n & 31;
    if ($mc==0||$mc==1||$mc==4||$mc==9||$mc==16||$mc==17||$mc==25) {
      my $sq = int(sqrt($n));
      return 1 if ($sq*$sq) == $n;
    }
  }
  0;
}

sub is_primitive_root {
  my($a, $n) = @_;
  $n = -$n if $n < 0;  # Ignore sign of n
  return ($n==1) ? 1 : 0 if $n <= 1;
  $a %= $n if $a < 0 || $a >= $n;

  return Math::Prime::Util::GMP::is_primitive_root($a,$n)
    if $Math::Prime::Util::_GMPfunc{"is_primitive_root"};

  if ($Math::Prime::Util::_GMPfunc{"znorder"} && $Math::Prime::Util::_GMPfunc{"totient"}) {
    my $order = Math::Prime::Util::GMP::znorder($a,$n);
    return 0 unless defined $order;
    my $totient = Math::Prime::Util::GMP::totient($n);
    return ($order eq $totient) ? 1 : 0;
  }

  return 0 if Math::Prime::Util::gcd($a, $n) != 1;
  my $s = Math::Prime::Util::euler_phi($n);
  return 0 if ($s % 2) == 0 && Math::Prime::Util::powmod($a, $s/2, $n) == 1;
  return 0 if ($s % 3) == 0 && Math::Prime::Util::powmod($a, $s/3, $n) == 1;
  return 0 if ($s % 5) == 0 && Math::Prime::Util::powmod($a, $s/5, $n) == 1;
  foreach my $f (Math::Prime::Util::factor_exp($s)) {
    my $fp = $f->[0];
    return 0 if $fp > 5 && Math::Prime::Util::powmod($a, $s/$fp, $n) == 1;
  }
  1;
}

sub znorder {
  my($a, $n) = @_;
  return if $n <= 0;
  return 1 if $n == 1;
  return if $a <= 0;
  return 1 if $a == 1;

  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::znorder($a,$n))
    if $Math::Prime::Util::_GMPfunc{"znorder"};

  # Sadly, Calc/FastCalc are horrendously slow for this function.
  return if Math::Prime::Util::gcd($a, $n) > 1;

  # The answer is one of the divisors of phi(n) and lambda(n).
  my $lambda = Math::Prime::Util::carmichael_lambda($n);
  $a = Math::BigInt->new("$a") unless ref($a) eq 'Math::BigInt';

  # This is easy and usually fast, but can bog down with too many divisors.
  if ($lambda <= 2**64) {
    foreach my $k (Math::Prime::Util::divisors($lambda)) {
      return $k if Math::Prime::Util::powmod($a,$k,$n) == 1;
    }
    return;
  }

  # Algorithm 1.7 from A. Das applied to Carmichael Lambda.
  $lambda = Math::BigInt->new("$lambda") unless ref($lambda) eq 'Math::BigInt';
  my $k = Math::BigInt->bone;
  foreach my $f (Math::Prime::Util::factor_exp($lambda)) {
    my($pi, $ei, $enum) = (Math::BigInt->new("$f->[0]"), $f->[1], 0);
    my $phidiv = $lambda / ($pi**$ei);
    my $b = Math::Prime::Util::powmod($a,$phidiv,$n);
    while ($b != 1) {
      return if $enum++ >= $ei;
      $b = Math::Prime::Util::powmod($b,$pi,$n);
      $k *= $pi;
    }
  }
  $k = _bigint_to_int($k) if $k->bacmp(BMAX) <= 0;
  return $k;
}

sub _dlp_trial {
  my ($a,$g,$p,$limit) = @_;
  $limit = $p if !defined $limit || $limit > $p;
  my $t = $g->copy;

  if ($limit < 1_000_000_000) {
    for my $k (1 .. $limit) {
      return $k if $t == $a;
      $t = Math::Prime::Util::mulmod($t, $g, $p);
    }
    return 0;
  }

  for (my $k = BONE->copy; $k < $limit; $k->binc) {
    if ($t == $a) {
      $k = _bigint_to_int($k) if $k->bacmp(BMAX) <= 0;
      return $k;
    }
    $t->bmul($g)->bmod($p);
  }
  0;
}
sub _dlp_bsgs {
  my ($a,$g,$p,$n,$_verbose) = @_;
  my $invg = invmod($g, $p);
  return unless defined $invg;
  my $maxm = Math::Prime::Util::sqrtint($n)+1;
  my $b = ($p + $maxm - 1) / $maxm;
  # Limit for time and space.
  $b = ($b > 4_000_000) ? 4_000_000 : int("$b");
  $maxm = ($maxm > $b) ? $b : int("$maxm");

  my %hash;
  my $am = BONE->copy;
  my $gm = Math::Prime::Util::powmod($invg, $maxm, $p);
  my $key = $a->copy;
  my $r;

  foreach my $m (0 .. $b) {
    # Baby Step
    if ($m <= $maxm) {
      $r = $hash{"$am"};
      if (defined $r) {
        print "  bsgs found in stage 1 after $m tries\n" if $_verbose;
        $r = Math::Prime::Util::addmod($m, Math::Prime::Util::mulmod($r,$maxm,$p), $p);
        return $r;
      }
      $hash{"$am"} = $m;
      $am = Math::Prime::Util::mulmod($am,$g,$p);
      if ($am == $a) {
        print "  bsgs found during bs\n" if $_verbose;
        return $m+1;
      }
    }

    # Giant Step
    $r = $hash{"$key"};
    if (defined $r) {
      print "  bsgs found in stage 2 after $m tries\n" if $_verbose;
      $r = Math::Prime::Util::addmod($r, Math::Prime::Util::mulmod($m,$maxm,$p), $p);
      return $r;
    }
    $hash{"$key"} = $m if $m <= $maxm;
    $key = Math::Prime::Util::mulmod($key,$gm,$p);
  }
  0;
}

sub znlog {
  my ($a,$g,$p) =
    map { ref($_) eq 'Math::BigInt' ? $_ : Math::BigInt->new("$_") } @_;
  $a->bmod($p);
  $g->bmod($p);
  return 0 if $a == 1 || $g == 0 || $p < 2;
  my $_verbose = Math::Prime::Util::prime_get_config()->{'verbose'};

  # For large p, znorder can be very slow.  Do trial test first.
  my $x = _dlp_trial($a, $g, $p, 200);
  if ($x == 0) {
    my $n = znorder($g, $p);
    if (defined $n && $n > 1000) {
      $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
      $x = _dlp_bsgs($a, $g, $p, $n, $_verbose);
      $x = _bigint_to_int($x) if ref($x) && $x->bacmp(BMAX) <= 0;
      return $x if $x > 0 && $g->copy->bmodpow($x, $p) == $a;
      print "  BSGS giving up\n" if $x == 0 && $_verbose;
      print "  BSGS incorrect answer $x\n" if $x > 0 && $_verbose > 1;
    }
    $x = _dlp_trial($a,$g,$p);
  }
  $x = _bigint_to_int($x) if ref($x) && $x->bacmp(BMAX) <= 0;
  return ($x == 0) ? undef : $x;
}

sub znprimroot {
  my($n) = @_;
  $n = -$n if $n < 0;
  if ($n <= 4) {
    return if $n == 0;
    return $n-1;
  }
  return if $n % 4 == 0;
  my $a = 1;
  my $phi = $n-1;
  if (!is_prob_prime($n)) {
    $phi = euler_phi($n);
    # Check that a primitive root exists.
    return if $phi != Math::Prime::Util::carmichael_lambda($n);
  }
  my @exp = map { Math::BigInt->new("$_") }
            map { int($phi/$_->[0]) }
            Math::Prime::Util::factor_exp($phi);
  #print "phi: $phi  factors: ", join(",",factor($phi)), "\n";
  #print "  exponents: ", join(",", @exp), "\n";
  while (1) {
    my $fail = 0;
    do { $a++ } while Math::Prime::Util::kronecker($a,$n) == 0;
    return if $a >= $n;
    foreach my $f (@exp) {
      if (Math::Prime::Util::powmod($a,$f,$n) == 1) {
        $fail = 1;
        last;
      }
    }
    return $a if !$fail;
  }
}


# Find first D in sequence (5,-7,9,-11,13,-15,...) where (D|N) == -1
sub _lucas_selfridge_params {
  my($n) = @_;

  # D is typically quite small: 67 max for N < 10^19.  However, it is
  # theoretically possible D could grow unreasonably.  I'm giving up at 4000M.
  my $d = 5;
  my $sign = 1;
  while (1) {
    my $gcd = (ref($n) eq 'Math::BigInt') ? Math::BigInt::bgcd($d, $n)
                                          : _gcd_ui($d, $n);
    return (0,0,0) if $gcd > 1 && $gcd != $n;  # Found divisor $d
    my $j = kronecker($d * $sign, $n);
    last if $j == -1;
    $d += 2;
    croak "Could not find Jacobi sequence for $n" if $d > 4_000_000_000;
    $sign = -$sign;
  }
  my $D = $sign * $d;
  my $P = 1;
  my $Q = int( (1 - $D) / 4 );
  ($P, $Q, $D)
}

sub _lucas_extrastrong_params {
  my($n, $increment) = @_;
  $increment = 1 unless defined $increment;

  my ($P, $Q, $D) = (3, 1, 5);
  while (1) {
    my $gcd = (ref($n) eq 'Math::BigInt') ? Math::BigInt::bgcd($D, $n)
                                          : _gcd_ui($D, $n);
    return (0,0,0) if $gcd > 1 && $gcd != $n;  # Found divisor $d
    last if kronecker($D, $n) == -1;
    $P += $increment;
    croak "Could not find Jacobi sequence for $n" if $P > 65535;
    $D = $P*$P - 4;
  }
  ($P, $Q, $D);
}

# returns U_k, V_k, Q_k all mod n
sub lucas_sequence {
  my($n, $P, $Q, $k) = @_;

  croak "lucas_sequence: n must be >= 2" if $n < 2;
  croak "lucas_sequence: k must be >= 0" if $k < 0;
  croak "lucas_sequence: P out of range" if abs($P) >= $n;
  croak "lucas_sequence: Q out of range" if abs($Q) >= $n;

  if ($Math::Prime::Util::_GMPfunc{"lucas_sequence"} && $Math::Prime::Util::GMP::VERSION >= 0.30) {
    return map { ($_ > ''.~0) ? Math::BigInt->new(''.$_) : $_ }
           Math::Prime::Util::GMP::lucas_sequence($n, $P, $Q, $k);
  }

  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';

  my $ZERO = $n->copy->bzero;
  $P = $ZERO+$P unless ref($P) eq 'Math::BigInt';
  $Q = $ZERO+$Q unless ref($Q) eq 'Math::BigInt';
  my $D = $P*$P - BTWO*BTWO*$Q;
  if ($D->is_zero) {
    my $S = ($ZERO+$P) >> 1;
    my $U = $S->copy->bmodpow($k-1,$n)->bmul($k)->bmod($n);
    my $V = $S->copy->bmodpow($k,$n)->bmul(BTWO)->bmod($n);
    my $Qk = ($ZERO+$Q)->bmodpow($k, $n);
    return ($U, $V, $Qk);
  }
  my $U = BONE->copy;
  my $V = $P->copy;
  my $Qk = $Q->copy;

  return (BZERO->copy, BTWO->copy, $Qk) if $k == 0;
  $k = Math::BigInt->new("$k") unless ref($k) eq 'Math::BigInt';
  my $kstr = substr($k->as_bin, 2);
  my $bpos = 0;

  if (($n % 2)==0) {
    $P->bmod($n);
    $Q->bmod($n);
    my($Uh,$Vl, $Vh, $Ql, $Qh) = (BONE->copy, BTWO->copy, $P->copy, BONE->copy, BONE->copy);
    my ($b,$s) = (length($kstr)-1, 0);
    if ($kstr =~ /(0+)$/) { $s = length($1); }
    for my $bpos (0 .. $b-$s-1) {
      $Ql->bmul($Qh)->bmod($n);
      if (substr($kstr,$bpos,1)) {
        $Qh = $Ql * $Q;
        $Uh->bmul($Vh)->bmod($n);
        $Vl->bmul($Vh)->bsub($P * $Ql)->bmod($n);
        $Vh->bmul($Vh)->bsub(BTWO * $Qh)->bmod($n);
      } else {
        $Qh = $Ql->copy;
        $Uh->bmul($Vl)->bsub($Ql)->bmod($n);
        $Vh->bmul($Vl)->bsub($P * $Ql)->bmod($n);
        $Vl->bmul($Vl)->bsub(BTWO * $Ql)->bmod($n);
      }
    }
    $Ql->bmul($Qh);
    $Qh = $Ql * $Q;
    $Uh->bmul($Vl)->bsub($Ql)->bmod($n);
    $Vl->bmul($Vh)->bsub($P * $Ql)->bmod($n);
    $Ql->bmul($Qh)->bmod($n);
    for (1 .. $s) {
      $Uh->bmul($Vl)->bmod($n);
      $Vl->bmul($Vl)->bsub(BTWO * $Ql)->bmod($n);
      $Ql->bmul($Ql)->bmod($n);
    }
    ($U, $V, $Qk) = ($Uh, $Vl, $Ql);
  } elsif ($Q->is_one) {
    my $Dinverse = $D->copy->bmodinv($n);
    if ($P > BTWO && !$Dinverse->is_nan) {
      # Calculate V_k with U=V_{k+1}
      $U = $P->copy->bmul($P)->bsub(BTWO)->bmod($n);
      while (++$bpos < length($kstr)) {
        if (substr($kstr,$bpos,1)) {
          $V->bmul($U)->bsub($P  )->bmod($n);
          $U->bmul($U)->bsub(BTWO)->bmod($n);
        } else {
          $U->bmul($V)->bsub($P  )->bmod($n);
          $V->bmul($V)->bsub(BTWO)->bmod($n);
        }
      }
      # Crandall and Pomerance eq 3.13: U_n = D^-1 (2V_{n+1} - PV_n)
      $U = $Dinverse * (BTWO*$U - $P*$V);
    } else {
      while (++$bpos < length($kstr)) {
        $U->bmul($V)->bmod($n);
        $V->bmul($V)->bsub(BTWO)->bmod($n);
        if (substr($kstr,$bpos,1)) {
          my $T1 = $U->copy->bmul($D);
          $U->bmul($P)->badd( $V);
          $U->badd($n) if $U->is_odd;
          $U->brsft(BONE);
          $V->bmul($P)->badd($T1);
          $V->badd($n) if $V->is_odd;
          $V->brsft(BONE);
        }
      }
    }
  } else {
    my $qsign = ($Q == -1) ? -1 : 0;
    while (++$bpos < length($kstr)) {
      $U->bmul($V)->bmod($n);
      if    ($qsign ==  1) { $V->bmul($V)->bsub(BTWO)->bmod($n); }
      elsif ($qsign == -1) { $V->bmul($V)->badd(BTWO)->bmod($n); }
      else { $V->bmul($V)->bsub($Qk->copy->blsft(BONE))->bmod($n); }
      if (substr($kstr,$bpos,1)) {
        my $T1 = $U->copy->bmul($D);
        $U->bmul($P)->badd( $V);
        $U->badd($n) if $U->is_odd;
        $U->brsft(BONE);

        $V->bmul($P)->badd($T1);
        $V->badd($n) if $V->is_odd;
        $V->brsft(BONE);

        if ($qsign != 0) { $qsign = -1; }
        else             { $Qk->bmul($Qk)->bmul($Q)->bmod($n); }
      } else {
        if ($qsign != 0) { $qsign = 1; }
        else             { $Qk->bmul($Qk)->bmod($n); }
      }
    }
    if    ($qsign ==  1) { $Qk->bneg; }
    elsif ($qsign == -1) { $Qk = $n->copy->bdec; }
  }
  $U->bmod($n);
  $V->bmod($n);
  return ($U, $V, $Qk);
}
sub _lucasuv {
  my($P, $Q, $k) = @_;

  croak "lucas_sequence: k must be >= 0" if $k < 0;
  return (0,2) if $k == 0;

  $P = Math::BigInt->new("$P") unless ref($P) eq 'Math::BigInt';
  $Q = Math::BigInt->new("$Q") unless ref($Q) eq 'Math::BigInt';

  # Simple way, very slow as k increases:
  #my($U0, $U1) = (BZERO->copy, BONE->copy);
  #my($V0, $V1) = (BTWO->copy, Math::BigInt->new("$P"));
  #for (2 .. $k) {
  #  ($U0,$U1) = ($U1, $P*$U1 - $Q*$U0);
  #  ($V0,$V1) = ($V1, $P*$V1 - $Q*$V0);
  #}
  #return ($U1, $V1);

  my($Uh,$Vl, $Vh, $Ql, $Qh) = (BONE->copy, BTWO->copy, $P->copy, BONE->copy, BONE->copy);
  $k = Math::BigInt->new("$k") unless ref($k) eq 'Math::BigInt';
  my $kstr = substr($k->as_bin, 2);
  my ($n,$s) = (length($kstr)-1, 0);
  if ($kstr =~ /(0+)$/) { $s = length($1); }

  if ($Q == -1) {
    # This could be simplified, and it's running 10x slower than it should.
    my ($ql,$qh) = (1,1);
    for my $bpos (0 .. $n-$s-1) {
      $ql *= $qh;
      if (substr($kstr,$bpos,1)) {
        $qh = -$ql;
        $Uh->bmul($Vh);
        if ($ql == 1) {
          $Vl->bmul($Vh)->bsub( $P );
          $Vh->bmul($Vh)->badd( BTWO );
        } else {
          $Vl->bmul($Vh)->badd( $P );
          $Vh->bmul($Vh)->bsub( BTWO );
        }
      } else {
        $qh = $ql;
        if ($ql == 1) {
          $Uh->bmul($Vl)->bdec;
          $Vh->bmul($Vl)->bsub($P);
          $Vl->bmul($Vl)->bsub(BTWO);
        } else {
          $Uh->bmul($Vl)->binc;
          $Vh->bmul($Vl)->badd($P);
          $Vl->bmul($Vl)->badd(BTWO);
        }
      }
    }
    $ql *= $qh;
    $qh = -$ql;
    if ($ql == 1) {
      $Uh->bmul($Vl)->bdec;
      $Vl->bmul($Vh)->bsub($P);
    } else {
      $Uh->bmul($Vl)->binc;
      $Vl->bmul($Vh)->badd($P);
    }
    $ql *= $qh;
    for (1 .. $s) {
      $Uh->bmul($Vl);
      if ($ql == 1) { $Vl->bmul($Vl)->bsub(BTWO); $ql *= $ql; }
      else          { $Vl->bmul($Vl)->badd(BTWO); $ql *= $ql; }
    }
    return map { ($_ > ''.~0) ? Math::BigInt->new(''.$_) : $_ } ($Uh, $Vl);
  }

  for my $bpos (0 .. $n-$s-1) {
    $Ql->bmul($Qh);
    if (substr($kstr,$bpos,1)) {
      $Qh = $Ql * $Q;
      #$Uh = $Uh * $Vh;
      #$Vl = $Vh * $Vl - $P * $Ql;
      #$Vh = $Vh * $Vh - BTWO * $Qh;
      $Uh->bmul($Vh);
      $Vl->bmul($Vh)->bsub($P * $Ql);
      $Vh->bmul($Vh)->bsub(BTWO * $Qh);
    } else {
      $Qh = $Ql->copy;
      #$Uh = $Uh * $Vl - $Ql;
      #$Vh = $Vh * $Vl - $P * $Ql;
      #$Vl = $Vl * $Vl - BTWO * $Ql;
      $Uh->bmul($Vl)->bsub($Ql);
      $Vh->bmul($Vl)->bsub($P * $Ql);
      $Vl->bmul($Vl)->bsub(BTWO * $Ql);
    }
  }
  $Ql->bmul($Qh);
  $Qh = $Ql * $Q;
  $Uh->bmul($Vl)->bsub($Ql);
  $Vl->bmul($Vh)->bsub($P * $Ql);
  $Ql->bmul($Qh);
  for (1 .. $s) {
    $Uh->bmul($Vl);
    $Vl->bmul($Vl)->bsub(BTWO * $Ql);
    $Ql->bmul($Ql);
  }
  return map { ($_ > ''.~0) ? Math::BigInt->new(''.$_) : $_ } ($Uh, $Vl, $Ql);
}
sub lucasu { (_lucasuv(@_))[0] }
sub lucasv { (_lucasuv(@_))[1] }

sub is_lucas_pseudoprime {
  my($n) = @_;

  return 0+($n >= 2) if $n < 4;
  return 0 if ($n % 2) == 0 || _is_perfect_square($n);

  my ($P, $Q, $D) = _lucas_selfridge_params($n);
  return 0 if $D == 0;  # We found a divisor in the sequence
  die "Lucas parameter error: $D, $P, $Q\n" if ($D != $P*$P - 4*$Q);

  my($U, $V, $Qk) = lucas_sequence($n, $P, $Q, $n+1);
  return ($U == 0) ? 1 : 0;
}

sub is_strong_lucas_pseudoprime {
  my($n) = @_;

  return 0+($n >= 2) if $n < 4;
  return 0 if ($n % 2) == 0 || _is_perfect_square($n);

  my ($P, $Q, $D) = _lucas_selfridge_params($n);
  return 0 if $D == 0;  # We found a divisor in the sequence
  die "Lucas parameter error: $D, $P, $Q\n" if ($D != $P*$P - 4*$Q);

  my $m = $n+1;
  my($s, $k) = (0, $m);
  while ( $k > 0 && !($k % 2) ) {
    $s++;
    $k >>= 1;
  }
  my($U, $V, $Qk) = lucas_sequence($n, $P, $Q, $k);

  return 1 if $U == 0;
  $V = Math::BigInt->new("$V") unless ref($V) eq 'Math::BigInt';
  $Qk = Math::BigInt->new("$Qk") unless ref($Qk) eq 'Math::BigInt';
  foreach my $r (0 .. $s-1) {
    return 1 if $V->is_zero;
    if ($r < ($s-1)) {
      $V->bmul($V)->bsub(BTWO*$Qk)->bmod($n);
      $Qk->bmul($Qk)->bmod($n);
    }
  }
  return 0;
}

sub is_extra_strong_lucas_pseudoprime {
  my($n) = @_;

  return 0+($n >= 2) if $n < 4;
  return 0 if ($n % 2) == 0 || _is_perfect_square($n);

  my ($P, $Q, $D) = _lucas_extrastrong_params($n);
  return 0 if $D == 0;  # We found a divisor in the sequence
  die "Lucas parameter error: $D, $P, $Q\n" if ($D != $P*$P - 4*$Q);

  # We have to convert n to a bigint or Math::BigInt::GMP's stupid set_si bug
  # (RT 71548) will hit us and make the test $V == $n-2 always return false.
  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';

  my($s, $k) = (0, $n->copy->binc);
  while ($k->is_even && !$k->is_zero) {
    $s++;
    $k->brsft(BONE);
  }

  my($U, $V, $Qk) = lucas_sequence($n, $P, $Q, $k);

  $V = Math::BigInt->new("$V") unless ref($V) eq 'Math::BigInt';
  return 1 if $U == 0 && ($V == BTWO || $V == ($n - BTWO));
  foreach my $r (0 .. $s-2) {
    return 1 if $V->is_zero;
    $V->bmul($V)->bsub(BTWO)->bmod($n);
  }
  return 0;
}

sub is_almost_extra_strong_lucas_pseudoprime {
  my($n, $increment) = @_;
  $increment = 1 unless defined $increment;

  return 0+($n >= 2) if $n < 4;
  return 0 if ($n % 2) == 0 || _is_perfect_square($n);

  my ($P, $Q, $D) = _lucas_extrastrong_params($n, $increment);
  return 0 if $D == 0;  # We found a divisor in the sequence
  die "Lucas parameter error: $D, $P, $Q\n" if ($D != $P*$P - 4*$Q);

  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';

  my $ZERO = $n->copy->bzero;
  my $TWO = $ZERO->copy->binc->binc;
  my $V = $ZERO + $P;           # V_{k}
  my $W = $ZERO + $P*$P-$TWO;   # V_{k+1}
  my $kstr = substr($n->copy->binc()->as_bin, 2);
  $kstr =~ s/(0*)$//;
  my $s = length($1);
  my $bpos = 0;
  while (++$bpos < length($kstr)) {
    if (substr($kstr,$bpos,1)) {
      $V->bmul($W)->bsub($P  )->bmod($n);
      $W->bmul($W)->bsub($TWO)->bmod($n);
    } else {
      $W->bmul($V)->bsub($P  )->bmod($n);
      $V->bmul($V)->bsub($TWO)->bmod($n);
    }
  }

  return 1 if $V == 2 || $V == ($n-$TWO);
  foreach my $r (0 .. $s-2) {
    return 1 if $V->is_zero;
    $V->bmul($V)->bsub($TWO)->bmod($n);
  }
  return 0;
}

sub is_frobenius_khashin_pseudoprime {
  my($n) = @_;
  return 0+($n >= 2) if $n < 4;
  return 0 unless $n % 2;
  return 0 if _is_perfect_square($n);

  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';

  my($k,$c) = (2,1);
  if    ($n % 4 == 3) { $c = $n-1; }
  elsif ($n % 8 == 5) { $c = 2; }
  else {
    do {
      $c += 2;
      $k = kronecker($c, $n);
    } while $k == 1;
  }
  return 0 if $k == 0 || ($k == 2 && !($n % 3));;

  my $ea = ($k == 2) ? 2 : 1;
  my($ra,$rb,$a,$b,$d) = ($ea,1,$ea,1,$n-1);
  while (!$d->is_zero) {
    if ($d->is_odd()) {
      ($ra, $rb) = ( (($ra*$a)%$n + ((($rb*$b)%$n)*$c)%$n) % $n,
                     (($rb*$a)%$n + ($ra*$b)%$n) % $n );
    }
    $d >>= 1;
    if (!$d->is_zero) {
      ($a, $b) = ( (($a*$a)%$n + ((($b*$b)%$n)*$c)%$n) % $n,
                   (($b*$a)%$n + ($a*$b)%$n) % $n );
    }
  }
  return ($ra == $ea && $rb == $n-1) ? 1 : 0;
}

sub is_frobenius_underwood_pseudoprime {
  my($n) = @_;
  return 0+($n >= 2) if $n < 4;
  return 0 unless $n % 2;

  my($a, $temp1, $temp2);
  if ($n % 4 == 3) {
    $a = 0;
  } else {
    for ($a = 1; $a < 1000000; $a++) {
      next if $a==2 || $a==4 || $a==7 || $a==8 || $a==10 || $a==14 || $a==16 || $a==18;
      my $j = kronecker($a*$a - 4, $n);
      last if $j == -1;
      return 0 if $j == 0 || ($a == 20 && _is_perfect_square($n));
    }
  }
  $temp1 = Math::Prime::Util::gcd(($a+4)*(2*$a+5), $n);
  return 0 if $temp1 != 1 && $temp1 != $n;

  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
  my $ZERO = $n->copy->bzero;
  my $ONE = $ZERO->copy->binc;
  my $TWO = $ONE->copy->binc;
  my($s, $t) = ($ONE->copy, $TWO->copy);

  my $ap2 = $TWO + $a;
  my $np1string = substr( $n->copy->binc->as_bin, 2);
  my $np1len = length($np1string);

  foreach my $bit (1 .. $np1len-1) {
    $temp2 = $t+$t;
    $temp2 += ($s * $a)  if $a != 0;
    $temp1 = $temp2 * $s;
    $temp2 = $t - $s;
    $s += $t;
    $t = ($s * $temp2) % $n;
    $s = $temp1 % $n;
    if ( substr( $np1string, $bit, 1 ) ) {
      if ($a == 0)  { $temp1 = $s + $s; }
      else          { $temp1 = $s * $ap2; }
      $temp1 += $t;
      $t->badd($t)->bsub($s);   # $t = ($t+$t) - $s;
      $s = $temp1;
    }
  }
  $temp1 = (2*$a+5) % $n;
  return ($s == 0 && $t == $temp1) ? 1 : 0;
}

sub _perrin_signature {
  my($n) = @_;
  my @S = (1,$n-1,3, 3,0,2);
  return @S if $n <= 1;

  my @nbin = todigits($n,2);
  shift @nbin;

  while (@nbin) {
    my @T = map { addmod(addmod(Math::Prime::Util::mulmod($S[$_],$S[$_],$n), $n-$S[5-$_],$n), $n-$S[5-$_],$n); } 0..5;
    my $T01 = addmod($T[2], $n-$T[1], $n);
    my $T34 = addmod($T[5], $n-$T[4], $n);
    my $T45 = addmod($T34, $T[3], $n);
    if (shift @nbin) {
      @S = ($T[0], $T01, $T[1], $T[4], $T45, $T[5]);
    } else {
      @S = ($T01, $T[1], addmod($T01,$T[0],$n), $T34, $T[4], $T45);
    }
  }
  @S;
}

sub is_perrin_pseudoprime {
  my($n, $restrict) = @_;
  $restrict = 0 unless defined $restrict;
  return 0+($n >= 2) if $n < 4;
  return 0 if $restrict > 2 && ($n % 2) == 0;

  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';

  my @S = _perrin_signature($n);
  return 0 unless $S[4] == 0;
  return 1 if $restrict == 0;
  return 0 unless $S[1] == $n-1;
  return 1 if $restrict == 1;
  my $j = kronecker(-23,$n);
  if ($j == -1) {
    my $B = $S[2];
    my $B2 = mulmod($B,$B,$n);
    my $A = addmod(addmod(1,mulmod(3,$B,$n),$n),$n-$B2,$n);
    my $C = addmod(mulmod(3,$B2,$n),$n-2,$n);
    return 1 if $S[0] == $A && $S[2] == $B && $S[3] == $B && $S[5] == $C && $B != 3 && addmod(mulmod($B2,$B,$n),$n-$B,$n) == 1;
  } else {
    return 0 if $j == 0 && $n != 23 && $restrict > 2;
    return 1 if $S[0] == 1 && $S[2] == 3 && $S[3] == 3 && $S[5] == 2;
    return 1 if $S[0] == 0 && $S[5] == $n-1 && $S[2] != $S[3] && addmod($S[2],$S[3],$n) == $n-3 && mulmod(addmod($S[2],$n-$S[3],$n),addmod($S[2],$n-$S[3],$n),$n) == $n-(23%$n);
  }
  0;
}

sub is_catalan_pseudoprime {
  my($n) = @_;
  return 0+($n >= 2) if $n < 4;
  my $m = ($n-1)>>1;
  return (binomial($m<<1,$m) % $n) == (($m&1) ? $n-1 : 1) ? 1 : 0;
}

sub is_frobenius_pseudoprime {
  my($n, $P, $Q) = @_;
  ($P,$Q) = (0,0) unless defined $P && defined $Q;
  return 0+($n >= 2) if $n < 4;

  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
  return 0 if $n->is_even;

  my($k, $Vcomp, $D, $Du) = (0, 4);
  if ($P == 0 && $Q == 0) {
    ($P,$Q) = (-1,2);
    while ($k != -1) {
      $P += 2;
      $P = 5 if $P == 3;  # Skip 3
      $D = $P*$P-4*$Q;
      $Du = ($D >= 0) ? $D : -$D;
      last if $P >= $n || $Du >= $n;   # TODO: remove?
      $k = kronecker($D, $n);
      return 0 if $k == 0;
      return 0 if $P == 10001 && _is_perfect_square($n);
    }
  } else {
    $D = $P*$P-4*$Q;
    $Du = ($D >= 0) ? $D : -$D;
    croak "Frobenius invalid P,Q: ($P,$Q)" if _is_perfect_square($Du);
  }
  return (is_prime($n) ? 1 : 0) if $n <= $Du || $n <= abs($Q) || $n <= abs($P);
  return 0 if Math::Prime::Util::gcd(abs($P*$Q*$D), $n) > 1;

  if ($k == 0) {
    $k = kronecker($D, $n);
    return 0 if $k == 0;
    my $Q2 = (2*abs($Q)) % $n;
    $Vcomp = ($k == 1) ? 2 : ($Q >= 0) ? $Q2 : $n-$Q2;
  }

  my($U, $V, $Qk) = lucas_sequence($n, $P, $Q, $n-$k);
  return 1 if $U == 0 && $V == $Vcomp;
  0;
}

# Since people have graciously donated millions of CPU years to doing these
# tests, it would be rude of us not to use the results.  This means we don't
# actually use the pretest and Lucas-Lehmer test coded below for any reasonable
# size number.
# See: http://www.mersenne.org/report_milestones/
my %_mersenne_primes;
undef @_mersenne_primes{2,3,5,7,13,17,19,31,61,89,107,127,521,607,1279,2203,2281,3217,4253,4423,9689,9941,11213,19937,21701,23209,44497,86243,110503,132049,216091,756839,859433,1257787,1398269,2976221,3021377,6972593,13466917,20996011,24036583,25964951,30402457,32582657,37156667,42643801,43112609,57885161,74207281};

sub is_mersenne_prime {
  my $p = shift;

  # Use the known Mersenne primes
  return 1 if exists $_mersenne_primes{$p};
  return 0 if $p < 34007399; # GIMPS has checked all below
  # Past this we do a generic Mersenne prime test

  return 1 if $p == 2;
  return 0 unless is_prob_prime($p);
  return 0 if $p > 3 && $p % 4 == 3 && $p < ((~0)>>1) && is_prob_prime($p*2+1);
  my $mp = BONE->copy->blsft($p)->bdec;

  # Definitely faster than using Math::BigInt that doesn't have GMP.
  return (0 == (Math::Prime::Util::GMP::lucas_sequence($mp, 4, 1, $mp+1))[0])
    if $Math::Prime::Util::_GMPfunc{"lucas_sequence"};

  my $V = Math::BigInt->new(4);
  for my $k (3 .. $p) {
    $V->bmul($V)->bsub(BTWO)->bmod($mp);
  }
  return $V->is_zero;
}


my $_poly_bignum;
sub _poly_new {
  my @poly = @_;
  push @poly, 0 unless scalar @poly;
  if ($_poly_bignum) {
    @poly = map { (ref $_ eq 'Math::BigInt')
                  ?  $_->copy
                  :  Math::BigInt->new("$_"); } @poly;
  }
  return \@poly;
}

#sub _poly_print {
#  my($poly) = @_;
#  carp "poly has null top degree" if $#$poly > 0 && !$poly->[-1];
#  foreach my $d (reverse 1 .. $#$poly) {
#    my $coef = $poly->[$d];
#    print "", ($coef != 1) ? $coef : "", ($d > 1) ? "x^$d" : "x", " + "
#      if $coef;
#  }
#  my $p0 = $poly->[0] || 0;
#  print "$p0\n";
#}

sub _poly_mod_mul {
  my($px, $py, $r, $n) = @_;

  my $px_degree = $#$px;
  my $py_degree = $#$py;
  my @res = map { $_poly_bignum ? Math::BigInt->bzero : 0 } 0 .. $r-1;

  # convolve(px, py) mod (X^r-1,n)
  my @indices_y = grep { $py->[$_] } (0 .. $py_degree);
  foreach my $ix (0 .. $px_degree) {
    my $px_at_ix = $px->[$ix];
    next unless $px_at_ix;
    if ($_poly_bignum) {
      foreach my $iy (@indices_y) {
        my $rindex = ($ix + $iy) % $r;  # reduce mod X^r-1
        $res[$rindex]->badd($px_at_ix->copy->bmul($py->[$iy]))->bmod($n);
      }
    } else {
      foreach my $iy (@indices_y) {
        my $rindex = ($ix + $iy) % $r;  # reduce mod X^r-1
        $res[$rindex] = ($res[$rindex] + $px_at_ix * $py->[$iy]) % $n;
      }
    }
  }
  # In case we had upper terms go to zero after modulo, reduce the degree.
  pop @res while !$res[-1];
  return \@res;
}

sub _poly_mod_pow {
  my($pn, $power, $r, $mod) = @_;
  my $res = _poly_new(1);
  my $p = $power;

  while ($p) {
    $res = _poly_mod_mul($res, $pn, $r, $mod) if ($p & 1);
    $p >>= 1;
    $pn  = _poly_mod_mul($pn,  $pn, $r, $mod) if $p;
  }
  return $res;
}

sub _test_anr {
  my($a, $n, $r) = @_;
  my $pp = _poly_mod_pow(_poly_new($a, 1), $n, $r, $n);
  $pp->[$n % $r] = (($pp->[$n % $r] || 0) -  1) % $n;  # subtract X^(n%r)
  $pp->[      0] = (($pp->[      0] || 0) - $a) % $n;  # subtract a
  return 0 if scalar grep { $_ } @$pp;
  1;
}

sub is_aks_prime {
  my $n = shift;
  return 0 if $n < 2 || is_power($n);

  my($log2n, $limit);
  if ($n > 2**48) {
    do { require Math::BigFloat; Math::BigFloat->import(); }
      if !defined $Math::BigFloat::VERSION;
    # limit = floor( log2(n) * log2(n) ).  o_r(n) must be larger than this
    my $floatn = Math::BigFloat->new("$n");
    #my $sqrtn = _bigint_to_int($floatn->copy->bsqrt->bfloor);
    # The following line seems to trigger a memory leak in Math::BigFloat::blog
    # (the part where $MBI is copied to $int) if $n is a Math::BigInt::GMP.
    $log2n = $floatn->copy->blog(2);
    $limit = _bigint_to_int( ($log2n * $log2n)->bfloor );
  } else {
    $log2n = log($n)/log(2) + 0.0001;      # Error on large side.
    $limit = int( $log2n*$log2n + 0.0001 );
  }

  my $r = next_prime($limit);
  foreach my $f (@{primes(0,$r-1)}) {
    return 1 if $f == $n;
    return 0 if !($n % $f);
  }

  while ($r < $n) {
    return 0 if !($n % $r);
    #return 1 if $r >= $sqrtn;
    last if znorder($n, $r) > $limit;  # Note the arguments!
    $r = next_prime($r);
  }

  return 1 if $r >= $n;

  # Since r is a prime, phi(r) = r-1
  my $rlimit = (ref($log2n) eq 'Math::BigFloat')
             ? _bigint_to_int( Math::BigFloat->new("$r")->bdec()
                                           ->bsqrt->bmul($log2n)->bfloor)
             : int( (sqrt(($r-1)) * $log2n) + 0.001 );

  $_poly_bignum = 1;
  if ( $n < (MPU_HALFWORD-1) ) {
    $_poly_bignum = 0;
    #$n = _bigint_to_int($n) if ref($n) eq 'Math::BigInt';
  } else {
    $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
  }

  my $_verbose = Math::Prime::Util::prime_get_config()->{'verbose'};
  print "# aks r = $r  s = $rlimit\n" if $_verbose;
  local $| = 1 if $_verbose > 1;
  for (my $a = 1; $a <= $rlimit; $a++) {
    return 0 unless _test_anr($a, $n, $r);
    print "." if $_verbose > 1;
  }
  print "\n" if $_verbose > 1;

  return 1;
}


sub _basic_factor {
  # MODIFIES INPUT SCALAR
  return ($_[0] == 1) ? () : ($_[0])   if $_[0] < 4;

  my @factors;
  if (ref($_[0]) ne 'Math::BigInt') {
    while ( !($_[0] % 2) ) { push @factors, 2;  $_[0] = int($_[0] / 2); }
    while ( !($_[0] % 3) ) { push @factors, 3;  $_[0] = int($_[0] / 3); }
    while ( !($_[0] % 5) ) { push @factors, 5;  $_[0] = int($_[0] / 5); }
  } else {
    # Without this, the bdivs will try to convert the results to BigFloat
    # and lose precision.
    $_[0]->upgrade(undef) if ref($_[0]) && $_[0]->upgrade();
    if (!Math::BigInt::bgcd($_[0], B_PRIM235)->is_one) {
      while ( $_[0]->is_even)   { push @factors, 2;  $_[0]->brsft(BONE); }
      foreach my $div (3, 5) {
        my ($q, $r) = $_[0]->copy->bdiv($div);
        while ($r->is_zero) {
          push @factors, $div;
          $_[0] = $q;
          ($q, $r) = $_[0]->copy->bdiv($div);
        }
      }
    }
    $_[0] = _bigint_to_int($_[0]) if $] >= 5.008 && $_[0] <= BMAX;
  }

  if ( ($_[0] > 1) && _is_prime7($_[0]) ) {
    push @factors, $_[0];
    $_[0] = 1;
  }
  @factors;
}

sub trial_factor {
  my($n, $limit) = @_;

  # Don't use _basic_factor here -- they want a trial forced.
  my @factors;
  if ($n < 4) {
    @factors = ($n == 1) ? () : ($n);
    return @factors;
  }

  my $start_idx = 1;
  # Expand small primes if it would help.
  push @_primes_small, @{primes($_primes_small[-1]+1, 100_003)}
    if $n > 400_000_000
    && $_primes_small[-1] < 99_000
    && (!defined $limit || $limit > $_primes_small[-1]);

  # Do initial bigint reduction.  Hopefully reducing it to native int.
  if (ref($n) eq 'Math::BigInt') {
    $n = $n->copy;  # Don't modify their original input!
    my $newlim = $n->copy->bsqrt;
    $limit = $newlim if !defined $limit || $limit > $newlim;
    while ($start_idx <= $#_primes_small) {
      my $f = $_primes_small[$start_idx++];
      last if $f > $limit;
      if ($n->copy->bmod($f)->is_zero) {
        do {
          push @factors, $f;
          $n->bdiv($f)->bfloor();
        } while $n->copy->bmod($f)->is_zero;
        last if $n < BMAX;
        my $newlim = $n->copy->bsqrt;
        $limit = $newlim if $limit > $newlim;
      }
    }
    return @factors if $n->is_one;
    $n = _bigint_to_int($n) if $n <= BMAX;
    return (@factors,$n) if $start_idx <= $#_primes_small && $_primes_small[$start_idx] > $limit;
  }

  {
    my $newlim = (ref($n) eq 'Math::BigInt') ? $n->copy->bsqrt : int(sqrt($n) + 0.001);
    $limit = $newlim if !defined $limit || $limit > $newlim;
  }

  if (ref($n) ne 'Math::BigInt') {
    for my $i ($start_idx .. $#_primes_small) {
      my $p = $_primes_small[$i];
      last if $p > $limit;
      if (($n % $p) == 0) {
        do { push @factors, $p;  $n = int($n/$p); } while ($n % $p) == 0;
        last if $n == 1;
        my $newlim = int( sqrt($n) + 0.001);
        $limit = $newlim if $newlim < $limit;
      }
    }
    if ($_primes_small[-1] < $limit) {
      my $inc = (($_primes_small[-1] % 6) == 1) ? 4 : 2;
      my $p = $_primes_small[-1] + $inc;
      while ($p <= $limit) {
        if (($n % $p) == 0) {
          do { push @factors, $p;  $n = int($n/$p); } while ($n % $p) == 0;
          last if $n == 1;
          my $newlim = int( sqrt($n) + 0.001);
          $limit = $newlim if $newlim < $limit;
        }
        $p += ($inc ^= 6);
      }
    }
  } else {   # n is a bigint.  Use mod-210 wheel trial division.
    # Generating a wheel mod $w starting at $s:
    # mpu 'my($s,$w,$t)=(11,2*3*5); say join ",",map { ($t,$s)=($_-$s,$_); $t; } grep { gcd($_,$w)==1 } $s+1..$s+$w;'
    # Should start at $_primes_small[$start_idx], do 11 + next multiple of 210.
    my @incs = map { Math::BigInt->new($_) } (2,4,2,4,6,2,6,4,2,4,6,6,2,6,4,2,6,4,6,8,4,2,4,2,4,8,6,4,6,2,4,6,2,6,6,4,2,4,6,2,6,4,2,4,2,10,2,10);
    my $f = 11; while ($f <= $_primes_small[$start_idx-1]-210) { $f += 210; }
    ($f, $limit) = map { Math::BigInt->new("$_") } ($f, $limit);
    SEARCH: while ($f <= $limit) {
      foreach my $finc (@incs) {
        if ($n->copy->bmod($f)->is_zero && $f->bacmp($limit) <= 0) {
          my $sf = ($f <= BMAX) ? _bigint_to_int($f) : $f->copy;
          do {
            push @factors, $sf;
            $n->bdiv($f)->bfloor();
          } while $n->copy->bmod($f)->is_zero;
          last SEARCH if $n->is_one;
          my $newlim = $n->copy->bsqrt;
          $limit = $newlim if $limit > $newlim;
        }
        $f->badd($finc);
      }
    }
  }
  push @factors, $n  if $n > 1;
  @factors;
}

my $_holf_r;
my @_fsublist = (
  [ "pbrent 32k", sub { pbrent_factor (shift,   32*1024, 1, 1) } ],
  [ "p-1 1M",     sub { pminus1_factor(shift, 1_000_000, undef, 1); } ],
  [ "ECM 1k",     sub { ecm_factor    (shift,     1_000,   5_000, 15) } ],
  [ "pbrent 512k",sub { pbrent_factor (shift,  512*1024, 7, 1) } ],
  [ "p-1 4M",     sub { pminus1_factor(shift, 4_000_000, undef, 1); } ],
  [ "ECM 10k",    sub { ecm_factor    (shift,    10_000,  50_000, 10) } ],
  [ "pbrent 512k",sub { pbrent_factor (shift,  512*1024, 11, 1) } ],
  [ "HOLF 256k",  sub { holf_factor   (shift, 256*1024, $_holf_r); $_holf_r += 256*1024; } ],
  [ "p-1 20M",    sub { pminus1_factor(shift,20_000_000); } ],
  [ "ECM 100k",   sub { ecm_factor    (shift,   100_000, 800_000, 10) } ],
  [ "HOLF 512k",  sub { holf_factor   (shift, 512*1024, $_holf_r); $_holf_r += 512*1024; } ],
  [ "pbrent 2M",  sub { pbrent_factor (shift, 2048*1024, 13, 1) } ],
  [ "HOLF 2M",    sub { holf_factor   (shift, 2048*1024, $_holf_r); $_holf_r += 2048*1024; } ],
  [ "ECM 1M",     sub { ecm_factor    (shift, 1_000_000, 1_000_000, 10) } ],
  [ "p-1 100M",   sub { pminus1_factor(shift, 100_000_000, 500_000_000); } ],
);

sub factor {
  my($n) = @_;
  _validate_positive_integer($n);
  my @factors;

  if ($n < 4) {
    @factors = ($n == 1) ? () : ($n);
    return @factors;
  }
  $n = $n->copy if ref($n) eq 'Math::BigInt';
  my $lim = 4999;  # How much trial factoring to do

  # For native integers, we could save a little time by doing hardcoded trials
  # by 2-29 here.  Skipping it.

  push @factors, trial_factor($n, $lim);
  return @factors if $factors[-1] < $lim*$lim;
  $n = pop(@factors);

  my @nstack = ($n);
  while (@nstack) {
    $n = pop @nstack;
    # Don't use bignum on $n if it has gotten small enough.
    $n = _bigint_to_int($n) if ref($n) eq 'Math::BigInt' && $n <= BMAX;
    #print "Looking at $n with stack ", join(",",@nstack), "\n";
    while ( ($n >= ($lim*$lim)) && !_is_prime7($n) ) {
      my @ftry;
      $_holf_r = 1;
      foreach my $sub (@_fsublist) {
        last if scalar @ftry >= 2;
        print "  starting $sub->[0]\n" if Math::Prime::Util::prime_get_config()->{'verbose'} > 1;
        @ftry = $sub->[1]->($n);
      }
      if (scalar @ftry > 1) {
        #print "  split into ", join(",",@ftry), "\n";
        $n = shift @ftry;
        $n = _bigint_to_int($n) if ref($n) eq 'Math::BigInt' && $n <= BMAX;
        push @nstack, @ftry;
      } else {
        #warn "trial factor $n\n";
        push @factors, trial_factor($n);
        #print "  trial into ", join(",",@factors), "\n";
        $n = 1;
        last;
      }
    }
    push @factors, $n  if $n != 1;
  }
  @factors = sort {$a<=>$b} @factors;
  return @factors;
}

sub _found_factor {
  my($f, $n, $what, @factors) = @_;
  if ($f == 1 || $f == $n) {
    push @factors, $n;
  } else {
    # Perl 5.6.2 needs things spelled out for it.
    my $f2 = (ref($n) eq 'Math::BigInt') ? $n->copy->bdiv($f)->as_int
                                         : int($n/$f);
    push @factors, $f;
    push @factors, $f2;
    croak "internal error in $what" unless $f * $f2 == $n;
    # MPU::GMP prints this type of message if verbose, so do the same.
    print "$what found factor $f\n" if Math::Prime::Util::prime_get_config()->{'verbose'} > 0;
  }
  @factors;
}

# TODO:
sub squfof_factor { trial_factor(@_) }

sub prho_factor {
  my($n, $rounds, $pa, $skipbasic) = @_;
  $rounds = 4*1024*1024 unless defined $rounds;
  $pa = 3 unless defined $pa;

  my @factors;
  if (!$skipbasic) {
    @factors = _basic_factor($n);
    return @factors if $n < 4;
  }

  my $inloop = 0;
  my $U = 7;
  my $V = 7;

  if ( ref($n) eq 'Math::BigInt' ) {

    my $zero = $n->copy->bzero;
    $pa = $zero->badd("$pa");
    $U = $zero->copy->badd($U);
    $V = $zero->copy->badd($V);
    for my $i (1 .. $rounds) {
      # Would use bmuladd here, but old Math::BigInt's barf with scalar $pa.
      $U->bmul($U)->badd($pa)->bmod($n);
      $V->bmul($V)->badd($pa);
      $V->bmul($V)->badd($pa)->bmod($n);
      my $f = Math::BigInt::bgcd($U-$V, $n);
      if ($f->bacmp($n) == 0) {
        last if $inloop++;  # We've been here before
      } elsif (!$f->is_one) {
        return _found_factor($f, $n, "prho", @factors);
      }
    }

  } elsif ($n < MPU_HALFWORD) {

    my $inner = 32;
    $rounds = int( ($rounds + $inner-1) / $inner );
    while ($rounds-- > 0) {
      my($m, $oldU, $oldV, $f) = (1, $U, $V);
      for my $i (1 .. $inner) {
        $U = ($U * $U + $pa) % $n;
        $V = ($V * $V + $pa) % $n;
        $V = ($V * $V + $pa) % $n;
        $f = ($U > $V) ? $U-$V : $V-$U;
        $m = ($m * $f) % $n;
      }
      $f = _gcd_ui( $m, $n );
      next if $f == 1;
      if ($f == $n) {
        ($U, $V) = ($oldU, $oldV);
        for my $i (1 .. $inner) {
          $U = ($U * $U + $pa) % $n;
          $V = ($V * $V + $pa) % $n;
          $V = ($V * $V + $pa) % $n;
          $f = ($U > $V) ? $U-$V : $V-$U;
          $f = _gcd_ui( $f, $n);
          last if $f != 1;
        }
        last if $f == 1 || $f == $n;
      }
      return _found_factor($f, $n, "prho", @factors);
    }

  } else {

    for my $i (1 .. $rounds) {
      if ($n <= (~0 >> 1)) {
       $U = _mulmod($U, $U, $n);  $U += $pa;  $U -= $n if $U >= $n;
       $V = _mulmod($V, $V, $n);  $V += $pa;  # Let the mulmod handle it
       $V = _mulmod($V, $V, $n);  $V += $pa;  $V -= $n if $V >= $n;
      } else {
       #$U = _mulmod($U, $U, $n); $U=$n-$U; $U = ($pa>=$U) ? $pa-$U : $n-$U+$pa;
       #$V = _mulmod($V, $V, $n); $V=$n-$V; $V = ($pa>=$V) ? $pa-$V : $n-$V+$pa;
       #$V = _mulmod($V, $V, $n); $V=$n-$V; $V = ($pa>=$V) ? $pa-$V : $n-$V+$pa;
       $U = _mulmod($U, $U, $n);  $U = _addmod($U, $pa, $n);
       $V = _mulmod($V, $V, $n);  $V = _addmod($V, $pa, $n);
       $V = _mulmod($V, $V, $n);  $V = _addmod($V, $pa, $n);
      }
      my $f = _gcd_ui( $U-$V,  $n );
      if ($f == $n) {
        last if $inloop++;  # We've been here before
      } elsif ($f != 1) {
        return _found_factor($f, $n, "prho", @factors);
      }
    }

  }
  push @factors, $n;
  @factors;
}

sub pbrent_factor {
  my($n, $rounds, $pa, $skipbasic) = @_;
  $rounds = 4*1024*1024 unless defined $rounds;
  $pa = 3 unless defined $pa;

  my @factors;
  if (!$skipbasic) {
    @factors = _basic_factor($n);
    return @factors if $n < 4;
  }

  my $Xi = 2;
  my $Xm = 2;

  if ( ref($n) eq 'Math::BigInt' ) {

    # Same code as the GMP version, but runs *much* slower.  Even with
    # Math::BigInt::GMP it's >200x slower.  With the default Calc backend
    # it's thousands of times slower.
    my $inner = 32;
    my $zero = $n->copy->bzero;
    my $saveXi;
    my $f;
    $Xi = $zero->copy->badd($Xi);
    $Xm = $zero->copy->badd($Xm);
    $pa = $zero->copy->badd($pa);
    my $r = 1;
    while ($rounds > 0) {
      my $rleft = ($r > $rounds) ? $rounds : $r;
      while ($rleft > 0) {
        my $dorounds = ($rleft > $inner) ? $inner : $rleft;
        my $m = $zero->copy->bone;
        $saveXi = $Xi->copy;
        foreach my $i (1 .. $dorounds) {
          $Xi->bmul($Xi)->badd($pa)->bmod($n);
          $m->bmul($Xi->copy->bsub($Xm));
        }
        $rleft -= $dorounds;
        $rounds -= $dorounds;
        $m->bmod($n);
        $f = Math::BigInt::bgcd($m,  $n);
        last unless $f->is_one;
      }
      if ($f->is_one) {
        $r *= 2;
        $Xm = $Xi->copy;
        next;
      }
      if ($f == $n) {  # back up to determine the factor
        $Xi = $saveXi->copy;
        do {
          $Xi->bmul($Xi)->badd($pa)->bmod($n);
          $f = Math::BigInt::bgcd($Xm-$Xi, $n);
        } while ($f != 1 && $r-- != 0);
        last if $f == 1 || $f == $n;
      }
      return _found_factor($f, $n, "pbrent", @factors);
    }

  } elsif ($n < MPU_HALFWORD) {

    # Doing the gcd batching as above works pretty well here, but it's a lot
    # of code for not much gain for general users.
    for my $i (1 .. $rounds) {
      $Xi = ($Xi * $Xi + $pa) % $n;
      my $f = _gcd_ui( ($Xi>$Xm) ? $Xi-$Xm : $Xm-$Xi, $n);
      return _found_factor($f, $n, "pbrent", @factors) if $f != 1 && $f != $n;
      $Xm = $Xi if ($i & ($i-1)) == 0;  # i is a power of 2
    }

  } else {

    for my $i (1 .. $rounds) {
      $Xi = _addmod( _mulmod($Xi, $Xi, $n), $pa, $n);
      my $f = _gcd_ui( ($Xi>$Xm) ? $Xi-$Xm : $Xm-$Xi, $n);
      return _found_factor($f, $n, "pbrent", @factors) if $f != 1 && $f != $n;
      $Xm = $Xi if ($i & ($i-1)) == 0;  # i is a power of 2
    }

  }
  push @factors, $n;
  @factors;
}

sub pminus1_factor {
  my($n, $B1, $B2, $skipbasic) = @_;

  my @factors;
  if (!$skipbasic) {
    @factors = _basic_factor($n);
    return @factors if $n < 4;
  }

  if ( ref($n) ne 'Math::BigInt' ) {
    # Stage 1 only
    $B1 = 10_000_000 unless defined $B1;
    my $pa = 2;
    my $f = 1;
    my($pc_beg, $pc_end, @bprimes);
    $pc_beg = 2;
    $pc_end = $pc_beg + 100_000;
    my $sqrtb1 = int(sqrt($B1));
    while (1) {
      $pc_end = $B1 if $pc_end > $B1;
      @bprimes = @{ primes($pc_beg, $pc_end) };
      foreach my $q (@bprimes) {
        my $k = $q;
        if ($q <= $sqrtb1) {
          my $kmin = int($B1 / $q);
          while ($k <= $kmin) { $k *= $q; }
        }
        $pa = _powmod($pa, $k, $n);
        if ($pa == 0) { push @factors, $n; return @factors; }
        my $f = _gcd_ui( $pa-1, $n );
        return _found_factor($f, $n, "pminus1", @factors) if $f != 1;
      }
      last if $pc_end >= $B1;
      $pc_beg = $pc_end+1;
      $pc_end += 500_000;
    }
    push @factors, $n;
    return @factors;
  }

  # Stage 2 isn't really any faster than stage 1 for the examples I've tried.
  # Perl's overhead is greater than the savings of multiply vs. powmod

  if (!defined $B1) {
    for my $mul (1, 100, 1000, 10_000, 100_000, 1_000_000) {
      $B1 = 1000 * $mul;
      $B2 = 1*$B1;
      #warn "Trying p-1 with $B1 / $B2\n";
      my @nf = pminus1_factor($n, $B1, $B2);
      if (scalar @nf > 1) {
        push @factors, @nf;
        return @factors;
      }
    }
    push @factors, $n;
    return @factors;
  }
  $B2 = 1*$B1 unless defined $B2;

  my $one = $n->copy->bone;
  my ($j, $q, $saveq) = (32, 2, 2);
  my $t = $one->copy;
  my $pa = $one->copy->binc();
  my $savea = $pa->copy;
  my $f = $one->copy;
  my($pc_beg, $pc_end, @bprimes);

  $pc_beg = 2;
  $pc_end = $pc_beg + 100_000;
  while (1) {
    $pc_end = $B1 if $pc_end > $B1;
    @bprimes = @{ primes($pc_beg, $pc_end) };
    foreach my $q (@bprimes) {
      my($k, $kmin) = ($q, int($B1 / $q));
      while ($k <= $kmin) { $k *= $q; }
      $t *= $k;                         # accumulate powers for a
      if ( ($j++ % 64) == 0) {
        next if $pc_beg > 2 && ($j-1) % 256;
        $pa->bmodpow($t, $n);
        $t = $one->copy;
        if ($pa == 0) { push @factors, $n; return @factors; }
        $f = Math::BigInt::bgcd( $pa->copy->bdec, $n );
        last if $f == $n;
        return _found_factor($f, $n, "pminus1", @factors) unless $f->is_one;
        $saveq = $q;
        $savea = $pa->copy;
      }
    }
    $q = $bprimes[-1];
    last if !$f->is_one || $pc_end >= $B1;
    $pc_beg = $pc_end+1;
    $pc_end += 500_000;
  }
  undef @bprimes;
  $pa->bmodpow($t, $n);
  if ($pa == 0) { push @factors, $n; return @factors; }
  $f = Math::BigInt::bgcd( $pa-1, $n );
  if ($f == $n) {
    $q = $saveq;
    $pa = $savea->copy;
    while ($q <= $B1) {
      my ($k, $kmin) = ($q, int($B1 / $q));
      while ($k <= $kmin) { $k *= $q; }
      $pa->bmodpow($k, $n);
      my $f = Math::BigInt::bgcd( $pa-1, $n );
      if ($f == $n) { push @factors, $n; return @factors; }
      last if !$f->is_one;
      $q = next_prime($q);
    }
  }
  # STAGE 2
  if ($f->is_one && $B2 > $B1) {
    my $bm = $pa->copy;
    my $b = $one->copy;
    my @precomp_bm;
    $precomp_bm[0] = ($bm * $bm) % $n;
    foreach my $j (1..19) {
      $precomp_bm[$j] = ($precomp_bm[$j-1] * $bm * $bm) % $n;
    }
    $pa->bmodpow($q, $n);
    my $j = 1;
    $pc_beg = $q+1;
    $pc_end = $pc_beg + 100_000;
    while (1) {
      $pc_end = $B2 if $pc_end > $B2;
      @bprimes = @{ primes($pc_beg, $pc_end) };
      foreach my $i (0 .. $#bprimes) {
        my $diff = $bprimes[$i] - $q;
        $q = $bprimes[$i];
        my $qdiff = ($diff >> 1) - 1;
        if (!defined $precomp_bm[$qdiff]) {
          $precomp_bm[$qdiff] = $bm->copy->bmodpow($diff, $n);
        }
        $pa->bmul($precomp_bm[$qdiff])->bmod($n);
        if ($pa == 0) { push @factors, $n; return @factors; }
        $b->bmul($pa-1);
        if (($j++ % 128) == 0) {
          $b->bmod($n);
          $f = Math::BigInt::bgcd( $b, $n );
          last if !$f->is_one;
        }
      }
      last if !$f->is_one || $pc_end >= $B2;
      $pc_beg = $pc_end+1;
      $pc_end += 500_000;
    }
    $f = Math::BigInt::bgcd( $b, $n );
  }
  return _found_factor($f, $n, "pminus1", @factors);
}

sub holf_factor {
  my($n, $rounds, $startrounds) = @_;
  $rounds = 64*1024*1024 unless defined $rounds;
  $startrounds = 1 unless defined $startrounds;
  $startrounds = 1 if $startrounds < 1;

  my @factors = _basic_factor($n);
  return @factors if $n < 4;

  if ( ref($n) eq 'Math::BigInt' ) {
    for my $i ($startrounds .. $rounds) {
      my $ni = $n->copy->bmul($i);
      my $s = $ni->copy->bsqrt->bfloor->as_int;
      if ($s * $s == $ni) {
        # s^2 = n*i, so m = s^2 mod n = 0.  Hence f = GCD(n, s) = GCD(n, n*i)
        my $f = Math::BigInt::bgcd($ni, $n);
        return _found_factor($f, $n, "HOLF", @factors);
      }
      $s->binc;
      my $m = ($s * $s) - $ni;
      # Check for perfect square
      my $mc = _bigint_to_int($m & 31);
      next unless $mc==0||$mc==1||$mc==4||$mc==9||$mc==16||$mc==17||$mc==25;
      my $f = $m->copy->bsqrt->bfloor->as_int;
      next unless ($f*$f) == $m;
      $f = Math::BigInt::bgcd( ($s > $f) ? $s-$f : $f-$s,  $n);
      return _found_factor($f, $n, "HOLF ($i rounds)", @factors);
    }
  } else {
    for my $i ($startrounds .. $rounds) {
      my $s = int(sqrt($n * $i));
      $s++ if ($s * $s) != ($n * $i);
      my $m = ($s < MPU_HALFWORD) ? ($s*$s) % $n : _mulmod($s, $s, $n);
      # Check for perfect square
      my $mc = $m & 31;
      next unless $mc==0||$mc==1||$mc==4||$mc==9||$mc==16||$mc==17||$mc==25;
      my $f = int(sqrt($m));
      next unless $f*$f == $m;
      $f = _gcd_ui($s - $f,  $n);
      return _found_factor($f, $n, "HOLF ($i rounds)", @factors);
    }
  }
  push @factors, $n;
  @factors;
}

sub fermat_factor {
  my($n, $rounds) = @_;
  $rounds = 64*1024*1024 unless defined $rounds;

  my @factors = _basic_factor($n);
  return @factors if $n < 4;

  if ( ref($n) eq 'Math::BigInt' ) {
    my $pa = $n->copy->bsqrt->bfloor->as_int;
    return _found_factor($pa, $n, "Fermat", @factors) if $pa*$pa == $n;
    $pa++;
    my $b2 = $pa*$pa - $n;
    my $lasta = $pa + $rounds;
    while ($pa <= $lasta) {
      my $mc = _bigint_to_int($b2 & 31);
      if ($mc==0||$mc==1||$mc==4||$mc==9||$mc==16||$mc==17||$mc==25) {
        my $s = $b2->copy->bsqrt->bfloor->as_int;
        if ($s*$s == $b2) {
          my $i = $pa-($lasta-$rounds)+1;
          return _found_factor($pa - $s, $n, "Fermat ($i rounds)", @factors);
        }
      }
      $pa++;
      $b2 = $pa*$pa-$n;
    }
  } else {
    my $pa = int(sqrt($n));
    return _found_factor($pa, $n, "Fermat", @factors) if $pa*$pa == $n;
    $pa++;
    my $b2 = $pa*$pa - $n;
    my $lasta = $pa + $rounds;
    while ($pa <= $lasta) {
      my $mc = $b2 & 31;
      if ($mc==0||$mc==1||$mc==4||$mc==9||$mc==16||$mc==17||$mc==25) {
        my $s = int(sqrt($b2));
        if ($s*$s == $b2) {
          my $i = $pa-($lasta-$rounds)+1;
          return _found_factor($pa - $s, $n, "Fermat ($i rounds)", @factors);
        }
      }
      $pa++;
      $b2 = $pa*$pa-$n;
    }
  }
  push @factors, $n;
  @factors;
}


sub ecm_factor {
  my($n, $B1, $B2, $ncurves) = @_;
  _validate_positive_integer($n);

  my @factors = _basic_factor($n);
  return @factors if $n < 4;

  if ($Math::Prime::Util::_GMPfunc{"ecm_factor"}) {
    $B1 = 0 if !defined $B1;
    $ncurves = 0 if !defined $ncurves;
    my @ef = Math::Prime::Util::GMP::ecm_factor($n, $B1, $ncurves);
    if (@ef > 1) {
      my $ecmfac = Math::Prime::Util::_reftyped($n, $ef[-1]);
      return _found_factor($ecmfac, $n, "ECM (GMP) B1=$B1 curves $ncurves", @factors);
    }
    push @factors, $n;
    return @factors;
  }

  $ncurves = 10 unless defined $ncurves;

  if (!defined $B1) {
    for my $mul (1, 10, 100, 1000, 10_000, 100_000, 1_000_000) {
      $B1 = 100 * $mul;
      $B2 = 10*$B1;
      #warn "Trying ecm with $B1 / $B2\n";
      my @nf = ecm_factor($n, $B1, $B2, $ncurves);
      if (scalar @nf > 1) {
        push @factors, @nf;
        return @factors;
      }
    }
    push @factors, $n;
    return @factors;
  }

  $B2 = 10*$B1 unless defined $B2;
  my $sqrt_b1 = int(sqrt($B1)+1);

  # Affine code.  About 3x slower than the projective, and no stage 2.
  #
  #if (!defined $Math::Prime::Util::ECAffinePoint::VERSION) {
  #  eval { require Math::Prime::Util::ECAffinePoint; 1; }
  #  or do { croak "Cannot load Math::Prime::Util::ECAffinePoint"; };
  #}
  #my @bprimes = @{ primes(2, $B1) };
  #my $irandf = Math::Prime::Util::_get_rand_func();
  #foreach my $curve (1 .. $ncurves) {
  #  my $a = $irandf->($n-1);
  #  my $b = 1;
  #  my $ECP = Math::Prime::Util::ECAffinePoint->new($a, $b, $n, 0, 1);
  #  foreach my $q (@bprimes) {
  #    my $k = $q;
  #    if ($k < $sqrt_b1) {
  #      my $kmin = int($B1 / $q);
  #      while ($k <= $kmin) { $k *= $q; }
  #    }
  #    $ECP->mul($k);
  #    my $f = $ECP->f;
  #    if ($f != 1) {
  #      last if $f == $n;
  #      warn "ECM found factors with B1 = $B1 in curve $curve\n";
  #      return _found_factor($f, $n, "ECM B1=$B1 curve $curve", @factors);
  #    }
  #    last if $ECP->is_infinity;
  #  }
  #}

  require Math::Prime::Util::ECProjectivePoint;
  require Math::Prime::Util::RandomPrimes;

  # With multiple curves, it's better to get all the primes at once.
  # The downside is this can kill memory with a very large B1.
  my @bprimes = @{ primes(3, $B1) };
  foreach my $q (@bprimes) {
    last if $q > $sqrt_b1;
    my($k,$kmin) = ($q, int($B1/$q));
    while ($k <= $kmin) { $k *= $q; }
    $q = $k;
  }
  my @b2primes = ($B2 > $B1) ? @{primes($B1+1, $B2)} : ();

  foreach my $curve (1 .. $ncurves) {
    my $sigma = Math::Prime::Util::urandomm($n-6) + 6;
    my ($u, $v) = ( ($sigma*$sigma - 5) % $n, (4 * $sigma) % $n );
    my ($x, $z) = ( ($u*$u*$u) % $n,  ($v*$v*$v) % $n );
    my $cb = (4 * $x * $v) % $n;
    my $ca = ( (($v-$u)**3) * (3*$u + $v) ) % $n;
    my $f = Math::BigInt::bgcd( $cb, $n );
    $f = Math::BigInt::bgcd( $z, $n ) if $f == 1;
    next if $f == $n;
    return _found_factor($f,$n, "ECM B1=$B1 curve $curve", @factors) if $f != 1;
    $cb = Math::BigInt->new("$cb") unless ref($cb) eq 'Math::BigInt';
    $u = $cb->copy->bmodinv($n);
    $ca = (($ca*$u) - 2) % $n;

    my $ECP = Math::Prime::Util::ECProjectivePoint->new($ca, $n, $x, $z);
    my $fm = $n-$n+1;
    my $i = 15;

    for (my $q = 2; $q < $B1; $q *= 2) { $ECP->double(); }
    foreach my $k (@bprimes) {
      $ECP->mul($k);
      $fm = ($fm * $ECP->x() ) % $n;
      if ($i++ % 32 == 0) {
        $f = Math::BigInt::bgcd($fm, $n);
        last if $f != 1;
      }
    }
    $f = Math::BigInt::bgcd($fm, $n);
    next if $f == $n;

    if ($f == 1 && $B2 > $B1) { # BEGIN STAGE 2
      my $D = int(sqrt($B2/2));  $D++ if $D % 2;
      my $one = $n - $n + 1;
      my $g = $one;

      my $S2P = $ECP->copy->normalize;
      $f = $S2P->f;
      if ($f != 1) {
        next if $f == $n;
        #warn "ECM S2 normalize f=$f\n" if $f != 1;
        return _found_factor($f, $n, "ECM S2 B1=$B1 curve $curve");
      }
      my $S2x = $S2P->x;
      my $S2d = $S2P->d;
      my @nqx = ($n-$n, $S2x);

      foreach my $i (2 .. 2*$D) {
        my($x2, $z2);
        if ($i % 2) {
          ($x2, $z2) = Math::Prime::Util::ECProjectivePoint::_addx($nqx[($i-1)/2], $nqx[($i+1)/2], $S2x, $n);
        } else {
          ($x2, $z2) = Math::Prime::Util::ECProjectivePoint::_double($nqx[$i/2], $one, $n, $S2d);
        }
        $nqx[$i] = $x2;
        #($f, $u, undef) = _extended_gcd($z2, $n);
        $f = Math::BigInt::bgcd( $z2, $n );
        last if $f != 1;
        $u = $z2->copy->bmodinv($n);
        $nqx[$i] = ($x2 * $u) % $n;
      }
      if ($f != 1) {
        next if $f == $n;
        #warn "ECM S2 1: B1 $B1 B2 $B2 curve $curve f=$f\n";
        return _found_factor($f, $n, "ECM S2 B1=$B1 curve $curve", @factors);
      }

      $x = $nqx[2*$D-1];
      my $m = 1;
      while ($m < ($B2+$D)) {
        if ($m != 1) {
          my $oldx = $S2x;
          my ($x1, $z1) = Math::Prime::Util::ECProjectivePoint::_addx($nqx[2*$D], $S2x, $x, $n);
          $f = Math::BigInt::bgcd( $z1, $n );
          last if $f != 1;
          $u = $z1->copy->bmodinv($n);
          $S2x = ($x1 * $u) % $n;
          $x = $oldx;
          last if $f != 1;
        }
        if ($m+$D > $B1) {
          my @p = grep { $_ >= $m-$D && $_ <= $m+$D } @b2primes;
          foreach my $i (@p) {
            last if $i >= $m;
            $g = ($g * ($S2x - $nqx[$m+$D-$i])) % $n;
          }
          foreach my $i (@p) {
            next unless $i > $m;
            next if $i > ($m+$m) || is_prime($m+$m-$i);
            $g = ($g * ($S2x - $nqx[$i-$m])) % $n;
          }
          $f = Math::BigInt::bgcd($g, $n);
          #warn "ECM S2 3: found $f in stage 2\n" if $f != 1;
          last if $f != 1;
        }
        $m += 2*$D;
      }
    } # END STAGE 2

    next if $f == $n;
    if ($f != 1) {
      #warn "ECM found factors with B1 = $B1 in curve $curve\n";
      return _found_factor($f, $n, "ECM B1=$B1 curve $curve", @factors);
    }
    # end of curve loop
  }
  push @factors, $n;
  @factors;
}

sub divisors {
  my($n) = @_;
  _validate_positive_integer($n);
  my(@factors, @d, @t);

  # In scalar context, returns sigma_0(n).  Very fast.
  return Math::Prime::Util::divisor_sum($n,0) unless wantarray;
  return ($n == 0) ? (0,1) : (1)  if $n <= 1;

  if ($Math::Prime::Util::_GMPfunc{"divisors"}) {
    # This trips an erroneous compile time error without the eval.
    eval ' @d = Math::Prime::Util::GMP::divisors($n); ';  ## no critic qw(ProhibitStringyEval)
    @d = map { $_ <= ~0 ? $_ : ref($n)->new($_) } @d   if ref($n);
    return @d;
  }

  @factors = Math::Prime::Util::factor($n);
  return (1,$n) if scalar @factors == 1;

  my $bigint = ref($n);
  @factors = map { $bigint->new("$_") } @factors  if $bigint;
  @d = $bigint ? ($bigint->new(1)) : (1);

  while (my $p = shift @factors) {
    my $e = 1;
    while (@factors && $p == $factors[0]) { $e++; shift(@factors); }
    push @d,  @t = map { $_ * $p } @d;               # multiply through once
    push @d,  @t = map { $_ * $p } @t   for 2 .. $e; # repeat
  }

  @d = map { $_ <= INTMAX ? _bigint_to_int($_) : $_ } @d   if $bigint;
  @d = sort { $a <=> $b } @d;
  @d;
}


sub chebyshev_theta {
  my($n,$low) = @_;
  $low = 2 unless defined $low;
  my($sum,$high) = (0.0, 0);
  while ($low <= $n) {
    $high = $low + 1e6;
    $high = $n if $high > $n;
    $sum += log($_) for @{primes($low,$high)};
    $low = $high+1;
  }
  $sum;
}

sub chebyshev_psi {
  my($n) = @_;
  return 0 if $n <= 1;
  my ($sum, $logn, $sqrtn) = (0.0, log($n), int(sqrt($n)));

  # Sum the log of prime powers first
  for my $p (@{primes($sqrtn)}) {
    my $logp = log($p);
    $sum += $logp * int($logn/$logp+1e-15);
  }
  # The rest all have exponent 1: add them in using the segmenting theta code
  $sum += chebyshev_theta($n, $sqrtn+1);

  $sum;
}

sub hclassno {
  my $n = shift;

  return -1 if $n == 0;
  return 0 if $n < 0 || ($n % 4) == 1 || ($n % 4) == 2;
  return 2 * (2,3,6,6,6,8,12,9,6,12,18,12,8,12,18,18,12,15,24,12,6,24,30,20,12,12,24,24,18,24)[($n>>1)-1] if $n <= 60;

  my ($h, $square, $b, $b2) = (0, 0, $n & 1, ($n+1) >> 2);

  if ($b == 0) {
    my $lim = int(sqrt($b2));
    if (_is_perfect_square($b2)) {
      $square = 1;
      $lim--;
    }
    #$h += scalar(grep { $_ <= $lim } divisors($b2));
    for my $i (1 .. $lim) { $h++ unless $b2 % $i; }
    ($b,$b2) = (2, ($n+4) >> 2);
  }
  while ($b2 * 3 < $n) {
    $h++ unless $b2 % $b;
    my $lim = int(sqrt($b2));
    if (_is_perfect_square($b2)) {
      $h++;
      $lim--;
    }
    #$h += 2 * scalar(grep { $_ > $b && $_ <= $lim } divisors($b2));
    for my $i ($b+1 .. $lim) { $h += 2 unless $b2 % $i; }
    $b += 2;
    $b2 = ($n+$b*$b) >> 2;
  }
  return (($b2*3 == $n) ? 2*(3*$h+1) : $square ? 3*(2*$h+1) : 6*$h) << 1;
}

# Sigma method for prime powers
sub _taup {
  my($p, $e, $n) = @_;
  my($bp) = Math::BigInt->new("".$p);
  if ($e == 1) {
    return (0,1,-24,252,-1472,4830,-6048,-16744,84480)[$p] if $p <= 8;
    my $ds5  = $bp->copy->bpow( 5)->binc();  # divisor_sum(p,5)
    my $ds11 = $bp->copy->bpow(11)->binc();  # divisor_sum(p,11)
    my $s    = Math::BigInt->new("".vecsum(map { vecprod(BTWO,Math::Prime::Util::divisor_sum($_,5), Math::Prime::Util::divisor_sum($p-$_,5)) } 1..($p-1)>>1));
    $n = ( 65*$ds11 + 691*$ds5 - (691*252)*$s ) / 756;
  } else {
    my $t = Math::BigInt->new(""._taup($p,1));
    $n = $t->copy->bpow($e);
    if ($e == 2) {
      $n -= $bp->copy->bpow(11);
    } elsif ($e == 3) {
      $n -= BTWO * $t * $bp->copy->bpow(11);
    } else {
      $n += vecsum( map { vecprod( ($_&1) ? - BONE : BONE,
                                   $bp->copy->bpow(11*$_),
                                   binomial($e-$_, $e-2*$_),
                                   $t ** ($e-2*$_) ) } 1 .. ($e>>1) );
    }
  }
  $n = _bigint_to_int($n) if ref($n) && $n->bacmp(BMAX) <= 0;
  $n;
}

# Cohen's method using Hurwitz class numbers
# The two hclassno calls could be collapsed with some work
sub _tauprime {
  my $p = shift;
  return -24 if $p == 2;
  my $sum = Math::BigInt->new(0);
  if ($p < (MPU_32BIT ?  300  :  1600)) {
    my($p9,$pp7) = (9*$p, 7*$p*$p);
    for my $t (1 .. Math::Prime::Util::sqrtint($p)) {
      my $t2 = $t * $t;
      my $v = $p - $t2;
      $sum += $t2**3 * (4*$t2*$t2 - $p9*$t2 + $pp7) * (Math::Prime::Util::hclassno(4*$v) + 2 * Math::Prime::Util::hclassno($v));
    }
    $p = Math::BigInt->new("$p");
  } else {
    $p = Math::BigInt->new("$p");
    my($p9,$pp7) = (9*$p, 7*$p*$p);
    for my $t (1 .. Math::Prime::Util::sqrtint($p)) {
      my $t2 = Math::BigInt->new("$t") ** 2;
      my $v = $p - $t2;
      $sum += $t2**3 * (4*$t2*$t2 - $p9*$t2 + $pp7) * (Math::Prime::Util::hclassno(4*$v) + 2 * Math::Prime::Util::hclassno($v));
    }
  }
  28*$p**6 - 28*$p**5 - 90*$p**4 - 35*$p**3 - 1 - 32 * ($sum/3);
}

# Recursive method for handling prime powers
sub _taupower {
  my($p, $e) = @_;
  return 1 if $e <= 0;
  return _tauprime($p) if $e == 1;
  $p = Math::BigInt->new("$p");
  my($tp, $p11) = ( _tauprime($p), $p**11 );
  return $tp ** 2 - $p11 if $e == 2;
  return $tp ** 3 - 2 * $tp * $p11 if $e == 3;
  return $tp ** 4 - 3 * $tp**2 * $p11 + $p11**2 if $e == 4;
  # Recurse -3
  ($tp**3 - 2*$tp*$p11) * _taupower($p,$e-3) + ($p11*$p11 - $tp*$tp*$p11) * _taupower($p,$e-4);
}

sub ramanujan_tau {
  my $n = shift;
  return 0 if $n <= 0;

  # Use GMP if we have no XS or if size is small
  if ($n < 100000 || !Math::Prime::Util::prime_get_config()->{'xs'}) {
    if ($Math::Prime::Util::_GMPfunc{"ramanujan_tau"}) {
      return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::ramanujan_tau($n));
    }
  }

  # _taup is faster for small numbers, but gets very slow.  It's not a huge
  # deal, and the GMP code will probably get run for small inputs anyway.
  vecprod(map { _taupower($_->[0],$_->[1]) } Math::Prime::Util::factor_exp($n));
}

sub _Euler {
 my($dig) = @_;
 return Math::Prime::Util::GMP::Euler($dig)
   if $dig > 70 && $Math::Prime::Util::_GMPfunc{"Euler"};
 '0.57721566490153286060651209008240243104215933593992359880576723488486772677766467';
}
sub _Li2 {
 my($dig) = @_;
 return Math::Prime::Util::GMP::li(2,$dig)
   if $dig > 70 && $Math::Prime::Util::_GMPfunc{"li"};
 '1.04516378011749278484458888919461313652261557815120157583290914407501320521';
}

sub ExponentialIntegral {
  my($x) = @_;
  return - MPU_INFINITY if $x == 0;
  return 0              if $x == - MPU_INFINITY;
  return MPU_INFINITY   if $x == MPU_INFINITY;

  if ($Math::Prime::Util::_GMPfunc{"ei"}) {
    $x = Math::BigFloat->new("$x") if defined $bignum::VERSION && ref($x) ne 'Math::BigFloat';
    return 0.0 + Math::Prime::Util::GMP::ei($x,40) if !ref($x);
    my $str = Math::Prime::Util::GMP::ei($x, _find_big_acc($x));
    return $x->copy->bzero->badd($str);
  }

  $x = Math::BigFloat->new("$x") if defined $bignum::VERSION && ref($x) ne 'Math::BigFloat';

  my $tol = 1e-16;
  my $sum = 0.0;
  my($y, $t);
  my $c = 0.0;
  my $val; # The result from one of the four methods

  if ($x < -1) {
    # Continued fraction
    my $lc = 0;
    my $ld = 1 / (1 - $x);
    $val = $ld * (-exp($x));
    for my $n (1 .. 100000) {
      $lc = 1 / (2*$n + 1 - $x - $n*$n*$lc);
      $ld = 1 / (2*$n + 1 - $x - $n*$n*$ld);
      my $old = $val;
      $val *= $ld/$lc;
      last if abs($val - $old) <= ($tol * abs($val));
    }
  } elsif ($x < 0) {
    # Rational Chebyshev approximation
    my @C6p = ( -148151.02102575750838086,
                 150260.59476436982420737,
                  89904.972007457256553251,
                  15924.175980637303639884,
                   2150.0672908092918123209,
                    116.69552669734461083368,
                      5.0196785185439843791020);
    my @C6q = (  256664.93484897117319268,
                 184340.70063353677359298,
                  52440.529172056355429883,
                   8125.8035174768735759866,
                    750.43163907103936624165,
                     40.205465640027706061433,
                      1.0000000000000000000000);
    my $sumn = $C6p[0]-$x*($C6p[1]-$x*($C6p[2]-$x*($C6p[3]-$x*($C6p[4]-$x*($C6p[5]-$x*$C6p[6])))));
    my $sumd = $C6q[0]-$x*($C6q[1]-$x*($C6q[2]-$x*($C6q[3]-$x*($C6q[4]-$x*($C6q[5]-$x*$C6q[6])))));
    $val = log(-$x) - ($sumn / $sumd);
  } elsif ($x < -log($tol)) {
    # Convergent series
    my $fact_n = 1;
    $y = _Euler(18)-$c; $t = $sum+$y; $c = ($t-$sum)-$y; $sum = $t;
    $y = log($x)-$c; $t = $sum+$y; $c = ($t-$sum)-$y; $sum = $t;
    for my $n (1 .. 200) {
      $fact_n *= $x/$n;
      my $term = $fact_n / $n;
      $y = $term-$c; $t = $sum+$y; $c = ($t-$sum)-$y; $sum = $t;
      last if $term < $tol;
    }
    $val = $sum;
  } else {
    # Asymptotic divergent series
    my $invx = 1.0 / $x;
    my $term = $invx;
    $sum = 1.0 + $term;
    for my $n (2 .. 200) {
      my $last_term = $term;
      $term *= $n * $invx;
      last if $term < $tol;
      if ($term < $last_term) {
        $y = $term-$c; $t = $sum+$y; $c = ($t-$sum)-$y; $sum = $t;
      } else {
        $y = (-$last_term/3)-$c; $t = $sum+$y; $c = ($t-$sum)-$y; $sum = $t;
        last;
      }
    }
    $val = exp($x) * $invx * $sum;
  }
  $val;
}

sub LogarithmicIntegral {
  my($x,$opt) = @_;
  return 0              if $x == 0;
  return - MPU_INFINITY if $x == 1;
  return MPU_INFINITY   if $x == MPU_INFINITY;
  croak "Invalid input to LogarithmicIntegral:  x must be > 0" if $x <= 0;
  $opt = 0 unless defined $opt;

  if ($Math::Prime::Util::_GMPfunc{"li"}) {
    $x = Math::BigFloat->new("$x") if defined $bignum::VERSION && ref($x) ne 'Math::BigFloat';
    return 0.0 + Math::Prime::Util::GMP::li($x,40) if !ref($x);
    my $str = Math::Prime::Util::GMP::li($x, _find_big_acc($x));
    return $x->copy->bzero->badd($str);
  }

  if ($x == 2) {
    my $li2const = (ref($x) eq 'Math::BigFloat') ? Math::BigFloat->new(_Li2(_find_big_acc($x))) : 0.0+_Li2(30);
    return $li2const;
  }

  if (defined $bignum::VERSION) {
    # If bignum is on, always use Math::BigFloat.
    $x = Math::BigFloat->new("$x") if ref($x) ne 'Math::BigFloat';
  } elsif (ref($x)) {
    # bignum is off, use native if small, BigFloat otherwise.
    if ($x <= 1e16) {
      $x = _bigint_to_int($x);
    } else {
      $x = _upgrade_to_float($x) if ref($x) ne 'Math::BigFloat';
    }
  }
  # Make sure we preserve whatever accuracy setting the input was using.
  $x->accuracy($_[0]->accuracy) if ref($x) && ref($_[0]) =~ /^Math::Big/ && $_[0]->accuracy;

  # Do divergent series here for big inputs.  Common for big pc approximations.
  # Why is this here?
  #   1) exp(log(x)) results in a lot of lost precision
  #   2) exp(x) with lots of precision turns out to be really slow, and in
  #      this case it was unnecessary.
  my $tol = 1e-16;
  my $xdigits = 0;
  my $finalacc = 0;
  if (ref($x) =~ /^Math::Big/) {
    $xdigits = _find_big_acc($x);
    my $xlen = length($x->copy->bfloor->bstr());
    $xdigits = $xlen if $xdigits < $xlen;
    $finalacc = $xdigits;
    $xdigits += length(int(log(0.0+"$x"))) + 1;
    $tol = Math::BigFloat->new(10)->bpow(-$xdigits);
    $x->accuracy($xdigits);
  }
  my $logx = $xdigits ? $x->copy->blog(undef,$xdigits) : log($x);

  # TODO: See if we can tune this
  if (0 && $x >= 1) {
    _upgrade_to_float();
    my $sum = Math::BigFloat->new(0);
    my $inner_sum = Math::BigFloat->new(0);
    my $p = Math::BigFloat->new(-1);
    my $factorial = 1;
    my $power2 = 1;
    my $q;
    my $k = 0;
    my $neglogx = -$logx;
    for my $n (1 .. 1000) {
      $factorial = vecprod($factorial, $n);
      $q = vecprod($factorial, $power2);
      $power2 = vecprod(2, $power2);
      while ($k <= ($n-1)>>1) {
        $inner_sum += Math::BigFloat->new(1) / (2*$k+1);
        $k++;
      }
      $p *= $neglogx;
      my $term = ($p / $q) * $inner_sum;
      $sum += $term;
      last if abs($term) < $tol;
    }
    $sum *= sqrt($x);
    return 0.0+_Euler(18) + log($logx) + $sum unless ref($x)=~/^Math::Big/;
    my $val = Math::BigFloat->new(_Euler(40))->badd("".log($logx))->badd("$sum");
    $val->accuracy($finalacc) if $xdigits;
    return $val;
  }

  if ($x > 1e16) {
    my $invx = ref($logx) ? Math::BigFloat->bone / $logx : 1.0/$logx;
    # n = 0  =>  0!/(logx)^0 = 1/1 = 1
    # n = 1  =>  1!/(logx)^1 = 1/logx
    my $term = $invx;
    my $sum = 1.0 + $term;
    for my $n (2 .. 1000) {
      my $last_term = $term;
      $term *= $n * $invx;
      last if $term < $tol;
      if ($term < $last_term) {
        $sum += $term;
      } else {
        $sum -= ($last_term/3);
        last;
      }
      $term->bround($xdigits) if $xdigits;
    }
    $invx *= $sum;
    $invx *= $x;
    $invx->accuracy($finalacc) if ref($invx) && $xdigits;
    return $invx;
  }
  # Convergent series.
  if ($x >= 1) {
    my $fact_n = 1.0;
    my $nfac = 1.0;
    my $sum  = 0.0;
    for my $n (1 .. 200) {
      $fact_n *= $logx/$n;
      my $term = $fact_n / $n;
      $sum += $term;
      last if $term < $tol;
      $term->bround($xdigits) if $xdigits;
    }

    return 0.0+_Euler(18) + log($logx) + $sum unless ref($x) =~ /^Math::Big/;

    my $val = Math::BigFloat->new(_Euler(40))->badd("".log($logx))->badd("$sum");
    $val->accuracy($finalacc) if $xdigits;
    return $val;
  }

  ExponentialIntegral($logx);
}

# Riemann Zeta function for native integers.
my @_Riemann_Zeta_Table = (
  0.6449340668482264364724151666460251892,  # zeta(2) - 1
  0.2020569031595942853997381615114499908,
  0.0823232337111381915160036965411679028,
  0.0369277551433699263313654864570341681,
  0.0173430619844491397145179297909205279,
  0.0083492773819228268397975498497967596,
  0.0040773561979443393786852385086524653,
  0.0020083928260822144178527692324120605,
  0.0009945751278180853371459589003190170,
  0.0004941886041194645587022825264699365,
  0.0002460865533080482986379980477396710,
  0.0001227133475784891467518365263573957,
  0.0000612481350587048292585451051353337,
  0.0000305882363070204935517285106450626,
  0.0000152822594086518717325714876367220,
  0.0000076371976378997622736002935630292,
  0.0000038172932649998398564616446219397,
  0.0000019082127165539389256569577951013,
  0.0000009539620338727961131520386834493,
  0.0000004769329867878064631167196043730,
  0.0000002384505027277329900036481867530,
  0.0000001192199259653110730677887188823,
  0.0000000596081890512594796124402079358,
  0.0000000298035035146522801860637050694,
  0.0000000149015548283650412346585066307,
  0.0000000074507117898354294919810041706,
  0.0000000037253340247884570548192040184,
  0.0000000018626597235130490064039099454,
  0.0000000009313274324196681828717647350,
  0.0000000004656629065033784072989233251,
  0.0000000002328311833676505492001455976,
  0.0000000001164155017270051977592973835,
  0.0000000000582077208790270088924368599,
  0.0000000000291038504449709968692942523,
  0.0000000000145519218910419842359296322,
  0.0000000000072759598350574810145208690,
  0.0000000000036379795473786511902372363,
  0.0000000000018189896503070659475848321,
  0.0000000000009094947840263889282533118,
);


sub RiemannZeta {
  my($x) = @_;

  my $ix = ($x == int($x))  ?  "" . Math::BigInt->new($x)  :  0;

  # Try our GMP code if possible.
  if ($Math::Prime::Util::_GMPfunc{"zeta"}) {
    my($wantbf,$xdigits) = _bfdigits($x);
    # If we knew the *exact* number of zero digits, we could let GMP zeta
    # handle the correct rounding.  But we don't, so we have to go over.
    my $zero_dig = "".int($x / 3) - 1;
    my $strval = Math::Prime::Util::GMP::zeta($x, $xdigits + 8 + $zero_dig);
    if ($strval =~ s/^(1\.0*)/./) {
      $strval .= "e-".(length($1)-2) if length($1) > 2;
    } else {
      $strval =~ s/^(\d+)/$1-1/e;
    }

    return ($wantbf)  ?  Math::BigFloat->new($strval,$wantbf)  : 0.0 + $strval;
  }

  # If we need a bigfloat result, then call our PP routine.
  if (defined $bignum::VERSION || ref($x) =~ /^Math::Big/) {
    require Math::Prime::Util::ZetaBigFloat;
    return Math::Prime::Util::ZetaBigFloat::RiemannZeta($x);
  }

  # Native float results
  return 0.0 + $_Riemann_Zeta_Table[int($x)-2]
    if $x == int($x) && defined $_Riemann_Zeta_Table[int($x)-2];
  my $tol = 1.11e-16;

  # Series based on (2n)! / B_2n.
  # This is a simplification of the Cephes zeta function.
  my @A = (
      12.0,
     -720.0,
      30240.0,
     -1209600.0,
      47900160.0,
     -1892437580.3183791606367583212735166426,
      74724249600.0,
     -2950130727918.1642244954382084600497650,
      116467828143500.67248729113000661089202,
     -4597978722407472.6105457273596737891657,
      181521054019435467.73425331153534235290,
     -7166165256175667011.3346447367083352776,
      282908877253042996618.18640556532523927,
  );
  my $s = 0.0;
  my $rb = 0.0;
  foreach my $i (2 .. 10) {
    $rb = $i ** -$x;
    $s += $rb;
    return $s if abs($rb/$s) < $tol;
  }
  my $w = 10.0;
  $s = $s  +  $rb*$w/($x-1.0)  -  0.5*$rb;
  my $ra = 1.0;
  foreach my $i (0 .. 12) {
    my $k = 2*$i;
    $ra *= $x + $k;
    $rb /= $w;
    my $t = $ra*$rb/$A[$i];
    $s += $t;
    $t = abs($t/$s);
    last if $t < $tol;
    $ra *= $x + $k + 1.0;
    $rb /= $w;
  }
  return $s;
}

# Riemann R function
sub RiemannR {
  my($x) = @_;

  croak "Invalid input to ReimannR:  x must be > 0" if $x <= 0;

  # With MPU::GMP v0.49 this is fast.
  if ($Math::Prime::Util::_GMPfunc{"riemannr"}) {
    my($wantbf,$xdigits) = _bfdigits($x);
    my $strval = Math::Prime::Util::GMP::riemannr($x, $xdigits);
    return ($wantbf)  ?  Math::BigFloat->new($strval,$wantbf)  :  0.0 + $strval;
  }


# TODO: look into this as a generic solution
if (0 && $Math::Prime::Util::_GMPfunc{"zeta"}) {
  my($wantbf,$xdigits) = _bfdigits($x);
  $x = _upgrade_to_float($x);

  my $extra_acc = 4;
  $xdigits += $extra_acc;
  $x->accuracy($xdigits);

  my $logx = log($x);
  my $part_term = $x->copy->bone;
  my $sum = $x->copy->bone;
  my $tol = $x->copy->bone->brsft($xdigits-1, 10);
  my $bigk = $x->copy->bone;
  my $term;
  for my $k (1 .. 10000) {
    $part_term *= $logx / $bigk;
    my $zarg = $bigk->copy->binc;
    my $zeta = (RiemannZeta($zarg) * $bigk) + $bigk;
    #my $strval = Math::Prime::Util::GMP::zeta($k+1, $xdigits + int(($k+1) / 3));
    #my $zeta = Math::BigFloat->new($strval)->bdec->bmul($bigk)->badd($bigk);
    $term = $part_term / $zeta;
    $sum += $term;
    last if $term < ($tol * $sum);
    $bigk->binc;
  }
  $sum->bround($xdigits-$extra_acc);
  my $strval = "$sum";
  return ($wantbf)  ?  Math::BigFloat->new($strval,$wantbf)  :  0.0 + $strval;
}

  if (defined $bignum::VERSION || ref($x) =~ /^Math::Big/) {
    require Math::Prime::Util::ZetaBigFloat;
    return Math::Prime::Util::ZetaBigFloat::RiemannR($x);
  }

  my $sum = 0.0;
  my $tol = 1e-18;
  my($c, $y, $t) = (0.0);
  if ($x > 10**17) {
    my @mob = Math::Prime::Util::moebius(0,300);
    for my $k (1 .. 300) {
      next if $mob[$k] == 0;
      my $term = $mob[$k] / $k *
                 Math::Prime::Util::LogarithmicIntegral($x**(1.0/$k));
      $y = $term-$c; $t = $sum+$y; $c = ($t-$sum)-$y; $sum = $t;
      last if abs($term) < ($tol * abs($sum));
    }
  } else {
    $y = 1.0-$c; $t = $sum+$y; $c = ($t-$sum)-$y; $sum = $t;
    my $flogx = log($x);
    my $part_term = 1.0;
    for my $k (1 .. 10000) {
      my $zeta = ($k <= $#_Riemann_Zeta_Table)
                 ? $_Riemann_Zeta_Table[$k+1-2]    # Small k from table
                 : RiemannZeta($k+1);              # Large k from function
      $part_term *= $flogx / $k;
      my $term = $part_term / ($k + $k * $zeta);
      $y = $term-$c; $t = $sum+$y; $c = ($t-$sum)-$y; $sum = $t;
      last if $term < ($tol * $sum);
    }
  }
  return $sum;
}

sub LambertW {
  my $x = shift;
  croak "Invalid input to LambertW:  x must be >= -1/e" if $x < -0.36787944118;
  $x = _upgrade_to_float($x) if ref($x) eq 'Math::BigInt';
  my $xacc = ref($x) ? _find_big_acc($x) : 0;
  my $w;

  if ($Math::Prime::Util::_GMPfunc{"lambertw"}) {
    my $w = (!$xacc)
          ? 0.0 + Math::Prime::Util::GMP::lambertw($x)
          : $x->copy->bzero->badd(Math::Prime::Util::GMP::lambertw($x, $xacc));
    return $w;
  }

  # Approximation
  if ($x < -0.06) {
    my $ti = $x * 2 * exp($x-$x+1) + 2;
    return -1 if $ti <= 0;
    my $t  = sqrt($ti);
    $w = (-1 + 1/6*$t + (257/720)*$t*$t + (13/720)*$t*$t*$t) / (1 + (5/6)*$t + (103/720)*$t*$t);
  } elsif ($x < 1.363) {
    my $l1 = log($x + 1);
    $w = $l1 * (1 - log(1+$l1) / (2+$l1));
  } elsif ($x < 3.7) {
    my $l1 = log($x);
    my $l2 = log($l1);
    $w = $l1 - $l2 - log(1 - $l2/$l1)/2.0;
  } else {
    my $l1 = log($x);
    my $l2 = log($l1);
    my $d1 = 2 * $l1 * $l1;
    my $d2 = 3 * $l1 * $d1;
    my $d3 = 2 * $l1 * $d2;
    my $d4 = 5 * $l1 * $d3;
    $w = $l1 - $l2 + $l2/$l1 + $l2*($l2-2)/$d1
       + $l2*(6+$l2*(-9+2*$l2))/$d2
       + $l2*(-12+$l2*(36+$l2*(-22+3*$l2)))/$d3
       + $l2*(60+$l2*(-300+$l2*(350+$l2*(-125+12*$l2))))/$d4;
  }

  # Now iterate to get the answer
  #
  # Newton:
  #   $w = $w*(log($x) - log($w) + 1) / ($w+1);
  # Halley:
  #   my $e = exp($w);
  #   my $f = $w * $e - $x;
  #   $w -= $f / ($w*$e+$e - ($w+2)*$f/(2*$w+2));

  # Fritsch converges quadratically, so tolerance could be 4x smaller.  Use 2x.
  my $tol = ($xacc) ? 10**(-int(1+$xacc/2)) : 1e-16;
  $w->accuracy($xacc+10) if $xacc;
  for (1 .. 200) {
    last if $w == 0;
    my $w1 = $w + 1;
    my $zn = log($x/$w) - $w;
    my $qn = $w1 * 2 * ($w1+(2*$zn/3));
    my $en = ($zn/$w1) * ($qn-$zn)/($qn-$zn*2);
    my $wen = $w * $en;
    $w += $wen;
    last if abs($wen) < $tol;
  }
  $w->accuracy($xacc) if $xacc;

  $w;
}

my $_Pi = "3.141592653589793238462643383279503";
sub Pi {
  my $digits = shift;
  return 0.0+$_Pi unless $digits;
  return 0.0+sprintf("%.*lf", $digits-1, $_Pi) if $digits < 15;
  return _upgrade_to_float($_Pi, $digits) if $digits < 30;

  # Performance ranking:
  #   MPU::GMP         Uses AGM or Ramanujan/Chudnosky with binary splitting
  #   MPFR             Uses AGM, from 1x to 1/4x the above
  #   Perl AGM w/GMP   also AGM, nice growth rate, but slower than above
  #   C pidigits       much worse than above, but faster than the others
  #   Perl AGM         without Math::BigInt::GMP, it's sluggish
  #   Math::BigFloat   new versions use AGM, old ones are *very* slow
  #
  # With a few thousand digits, any of the top 4 are fine.
  # At 10k digits, the first two are pulling away.
  # At 50k digits, the first three are 5-20x faster than C pidigits, and
  #   pray you're not having to the Perl BigFloat methods without GMP.
  # At 100k digits, the first two are 15x faster than the third, C pidigits
  #   is 200x slower, and the rest thousands of times slower.
  # At 1M digits, the first is under 1 second, MPFR under 2 seconds,
  #   Perl AGM (Math::BigInt::GMP) is over a minute, and C piigits at 1.5 hours.
  #
  # Interestingly, Math::BigInt::Pari, while greatly faster than Calc, is
  # *much* slower than GMP for these operations (both AGM and Machin).  While
  # Perl AGM with the Math::BigInt::GMP backend will pull away from C pidigits,
  # using it with the other backends doesn't do so.
  #
  # The GMP program at https://gmplib.org/download/misc/gmp-chudnovsky.c
  # will run ~4x faster than MPFR and ~1.5x faster than MPU::GMP.

  my $have_bigint_gmp = Math::BigInt->config()->{lib} =~ /GMP/;
  my $have_xdigits    = Math::Prime::Util::prime_get_config()->{'xs'};
  my $_verbose = Math::Prime::Util::prime_get_config()->{'verbose'};

  if ($Math::Prime::Util::_GMPfunc{"Pi"}) {
    print "  using MPUGMP for Pi($digits)\n" if $_verbose;
    return _upgrade_to_float( Math::Prime::Util::GMP::Pi($digits) );
  }

  # We could consider looking for Math::MPFR or Math::Pari

  # This has a *much* better growth rate than the later solutions.
  if ( !$have_xdigits || ($have_bigint_gmp && $digits > 100) ) {
    print "  using Perl AGM for Pi($digits)\n" if $_verbose;
    # Brent-Salamin (aka AGM or Gauss-Legendre)
    $digits += 8;
    my $HALF = _upgrade_to_float(0.5);
    my ($an, $bn, $tn, $pn) = ($HALF->copy->bone, $HALF->copy->bsqrt($digits),
                               $HALF->copy->bmul($HALF), $HALF->copy->bone);
    while ($pn < $digits) {
      my $prev_an = $an->copy;
      $an->badd($bn)->bmul($HALF, $digits);
      $bn->bmul($prev_an)->bsqrt($digits);
      $prev_an->bsub($an);
      $tn->bsub($pn * $prev_an * $prev_an);
      $pn->badd($pn);
    }
    $an->badd($bn);
    $an->bmul($an,$digits)->bdiv(4*$tn, $digits-8);
    return $an;
  }

  # Spigot method in C.  Low overhead but not good growth rate.
  if ($have_xdigits) {
    print "  using XS spigot for Pi($digits)\n" if $_verbose;
    return _upgrade_to_float(Math::Prime::Util::_pidigits($digits));
  }

  # We're going to have to use the Math::BigFloat code.
  # 1) it rounds incorrectly (e.g. 761, 1372, 1509,...).
  #    Fix by adding some digits and rounding.
  # 2) AGM is *much* faster once past ~2000 digits
  # 3) It is very slow without the GMP backend.  The Pari backend helps
  #    but it still pretty bad.  With Calc it's glacial for large inputs.

  #           Math::BigFloat                AGM              spigot   AGM
  # Size     GMP    Pari  Calc        GMP    Pari  Calc        C      C+GMP
  #   500   0.04    0.60   0.30      0.08    0.10   0.47      0.09    0.06
  #  1000   0.04    0.11   1.82      0.09    0.14   1.82      0.09    0.06
  #  2000   0.07    0.37  13.5       0.09    0.34   9.16      0.10    0.06
  #  4000   0.14    2.17 107.8       0.12    1.14  39.7       0.20    0.06
  #  8000   0.52   15.7              0.22    4.63 186.2       0.56    0.08
  # 16000   2.73  121.8              0.52   19.2              2.00    0.08
  # 32000  15.4                      1.42                     7.78    0.12
  #                                   ^                        ^       ^
  #                                   |      use this THIRD ---+       |
  #                use this SECOND ---+                                |
  #                                                  use this FIRST ---+
  # approx
  # growth  5.6x    7.6x   8.0x      2.7x    4.1x   4.7x      3.9x    2.0x

  print "  using BigFloat for Pi($digits)\n" if $_verbose;
  _upgrade_to_float(0);
  return Math::BigFloat::bpi($digits+10)->round($digits);
}

sub forpart {
  my($sub, $n, $rhash) = @_;
  _forcompositions(1, $sub, $n, $rhash);
}
sub forcomp {
  my($sub, $n, $rhash) = @_;
  _forcompositions(0, $sub, $n, $rhash);
}
sub _forcompositions {
  my($ispart, $sub, $n, $rhash) = @_;
  _validate_positive_integer($n);
  my($mina, $maxa, $minn, $maxn, $primeq) = (1,$n,1,$n,-1);
  if (defined $rhash) {
    croak "forpart second argument must be a hash reference"
      unless ref($rhash) eq 'HASH';
    if (defined $rhash->{amin}) {
      $mina = $rhash->{amin};
      _validate_positive_integer($mina);
    }
    if (defined $rhash->{amax}) {
      $maxa = $rhash->{amax};
      _validate_positive_integer($maxa);
    }
    $minn = $maxn = $rhash->{n} if defined $rhash->{n};
    $minn = $rhash->{nmin} if defined $rhash->{nmin};
    $maxn = $rhash->{nmax} if defined $rhash->{nmax};
    _validate_positive_integer($minn);
    _validate_positive_integer($maxn);
    if (defined $rhash->{prime}) {
      $primeq = $rhash->{prime};
      _validate_positive_integer($primeq);
    }
   $mina = 1 if $mina < 1;
   $maxa = $n if $maxa > $n;
   $minn = 1 if $minn < 1;
   $maxn = $n if $maxn > $n;
   $primeq = 2 if $primeq != -1 && $primeq != 0;
  }

  $sub->() if $n == 0 && $minn <= 1;
  return if $n < $minn || $minn > $maxn || $mina > $maxa || $maxn <= 0 || $maxa <= 0;

  my $oldforexit = Math::Prime::Util::_start_for_loop();
  my ($x, $y, $r, $k);
  my @a = (0) x ($n);
  $k = 1;
  $a[0] = $mina - 1;
  $a[1] = $n - $mina + 1;
  while ($k != 0) {
    $x = $a[$k-1]+1;
    $y = $a[$k]-1;
    $k--;
    $r = $ispart ? $x : 1;
    while ($r <= $y) {
      $a[$k] = $x;
      $x = $r;
      $y -= $x;
      $k++;
    }
    $a[$k] = $x + $y;
    # Restrict size
    while ($k+1 > $maxn) {
      $a[$k-1] += $a[$k];
      $k--;
    }
    next if $k+1 < $minn;
    # Restrict values
    if ($mina > 1 || $maxa < $n) {
      last if $a[0] > $maxa;
      if ($ispart) {
        next if $a[$k] > $maxa;
      } else {
        next if Math::Prime::Util::vecany(sub{ $_ < $mina || $_ > $maxa }, @a[0..$k]);
      }
    }
    next if $primeq == 0 && Math::Prime::Util::vecany(sub{ is_prime($_) }, @a[0..$k]);
    next if $primeq == 2 && Math::Prime::Util::vecany(sub{ !is_prime($_) }, @a[0..$k]);
    last if Math::Prime::Util::_get_forexit();
    $sub->(@a[0 .. $k]);
  }
  Math::Prime::Util::_end_for_loop($oldforexit);
}
sub forcomb {
  my($sub, $n, $k) = @_;
  _validate_positive_integer($n);

  my($begk, $endk);
  if (defined $k) {
    _validate_positive_integer($k);
    return if $k > $n;
    $begk = $endk = $k;
  } else {
    $begk = 0;
    $endk = $n;
  }

  my $oldforexit = Math::Prime::Util::_start_for_loop();
  for my $k ($begk .. $endk) {
    if ($k == 0) {
      $sub->();
    } else {
      my @c = 0 .. $k-1;
      while (1) {
        $sub->(@c);
        last if Math::Prime::Util::_get_forexit();
        next if $c[-1]++ < $n-1;
        my $i = $k-2;
        $i-- while $i >= 0 && $c[$i] >= $n-($k-$i);
        last if $i < 0;
        $c[$i]++;
        while (++$i < $k) { $c[$i] = $c[$i-1] + 1; }
      }
    }
    last if Math::Prime::Util::_get_forexit();
  }
  Math::Prime::Util::_end_for_loop($oldforexit);
}
sub _forperm {
  my($sub, $n, $all_perm) = @_;
  my $k = $n;
  my @c = reverse 0 .. $k-1;
  my $inc = 0;
  my $send = 1;
  my $oldforexit = Math::Prime::Util::_start_for_loop();
  while (1) {
    if (!$all_perm) {   # Derangements via simple filtering.
      $send = 1;
      for my $p (0 .. $#c) {
        if ($c[$p] == $k-$p-1) {
          $send = 0;
          last;
        }
      }
    }
    if ($send) {
      $sub->(reverse @c);
      last if Math::Prime::Util::_get_forexit();
    }
    if (++$inc & 1) {
      @c[0,1] = @c[1,0];
      next;
    }
    my $j = 2;
    $j++ while $j < $k && $c[$j] > $c[$j-1];
    last if $j >= $k;
    my $m = 0;
    $m++ while $c[$j] > $c[$m];
    @c[$j,$m] = @c[$m,$j];
    @c[0..$j-1] = reverse @c[0..$j-1];
  }
  Math::Prime::Util::_end_for_loop($oldforexit);
}
sub forperm {
  my($sub, $n, $k) = @_;
  _validate_positive_integer($n);
  croak "Too many arguments for forperm" if defined $k;
  return $sub->() if $n == 0;
  return $sub->(0) if $n == 1;
  _forperm($sub, $n, 1);
}
sub forderange {
  my($sub, $n, $k) = @_;
  _validate_positive_integer($n);
  croak "Too many arguments for forderange" if defined $k;
  return $sub->() if $n == 0;
  return if $n == 1;
  _forperm($sub, $n, 0);
}

sub _multiset_permutations {
  my($sub, $prefix, $ar, $sum) = @_;

  return if $sum == 0;

  # Remove any values with 0 occurances
  my @n = grep { $_->[1] > 0 } @$ar;

  if ($sum == 1) {                       # A single value
    $sub->(@$prefix, $n[0]->[0]);
  } elsif ($sum == 2) {                  # Optimize the leaf case
    my($n0,$n1) = map { $_->[0] } @n;
    if (@n == 1) {
      $sub->(@$prefix, $n0, $n0);
    } else {
      $sub->(@$prefix, $n0, $n1);
      $sub->(@$prefix, $n1, $n0) unless Math::Prime::Util::_get_forexit();
    }
  } elsif (0 && $sum == scalar(@n)) {         # All entries have 1 occurance
    # TODO:  Figure out a way to use this safely.  We need to capture any
    #        lastfor that was seen in the forperm.
    my @i = map { $_->[0] } @n;
    Math::Prime::Util::forperm(sub { $sub->(@$prefix, @i[@_]) }, 1+$#i);
  } else {                               # Recurse over each leading value
    for my $v (@n) {
      $v->[1]--;
      push @$prefix, $v->[0];
      no warnings 'recursion';
      _multiset_permutations($sub, $prefix, \@n, $sum-1);
      pop @$prefix;
      $v->[1]++;
      last if Math::Prime::Util::_get_forexit();
    }
  }
}

sub numtoperm {
  my($n,$k) = @_;
  _validate_positive_integer($n);
  _validate_integer($k);
  return () if $n == 0;
  return (0) if $n == 1;
  my $f = factorial($n-1);
  $k %= vecprod($f,$n) if $k < 0 || int($k/$f) >= $n;
  my @S = map { $_ } 0 .. $n-1;
  my @V;
  while ($n-- > 0) {
    my $i = int($k/$f);
    push @V, splice(@S,$i,1);
    last if $n == 0;
    $k -= $i*$f;
    $f /= $n;
  }
  @V;
}

sub permtonum {
  my $A = shift;
  croak "permtonum argument must be an array reference"
    unless ref($A) eq 'ARRAY';
  my $n = scalar(@$A);
  return 0 if $n == 0;
  {
    my %S;
    for my $v (@$A) {
      croak "permtonum invalid permutation array"
        if !defined $v || $v < 0 || $v >= $n || $S{$v}++;
    }
  }
  my $f = factorial($n-1);
  my $rank = 0;
  for my $i (0 .. $n-2) {
    my $k = 0;
    for my $j ($i+1 .. $n-1) {
      $k++ if $A->[$j] < $A->[$i];
    }
    $rank = Math::Prime::Util::vecsum($rank, Math::Prime::Util::vecprod($k,$f));
    $f /= $n-$i-1;
  }
  $rank;
}

sub randperm {
  my($n,$k) = @_;
  _validate_positive_integer($n);
  if (defined $k) {
    _validate_positive_integer($k);
  }
  $k = $n if !defined($k) || $k > $n;
  return () if $k == 0;

  my @S;
  if ("$k"/"$n" <= 0.30) {
    my %seen;
    my $v;
    for my $i (1 .. $k) {
      do { $v = Math::Prime::Util::urandomm($n); } while $seen{$v}++;
      push @S,$v;
    }
  } else {
    @S = map { $_ } 0..$n-1;
    for my $i (0 .. $n-2) {
      last if $i >= $k;
      my $j = Math::Prime::Util::urandomm($n-$i);
      @S[$i,$i+$j] = @S[$i+$j,$i];
    }
    $#S = $k-1;
  }
  return @S;
}

sub shuffle {
  my @S=@_;
  # Note: almost all the time is spent in urandomm.
  for (my $i = $#S; $i >= 1; $i--) {
    my $j = Math::Prime::Util::urandomm($i+1);
    @S[$i,$j] = @S[$j,$i];
  }
  @S;
}

###############################################################################
#       Random numbers
###############################################################################

# PPFE:  irand irand64 drand random_bytes csrand srand _is_csprng_well_seeded
sub urandomb {
  my($n) = @_;
  return 0 if $n <= 0;
  return ( Math::Prime::Util::irand() >> (32-$n) ) if $n <= 32;
  return ( Math::Prime::Util::irand64() >> (64-$n) ) if MPU_MAXBITS >= 64 && $n <= 64;
  my $bytes = Math::Prime::Util::random_bytes(($n+7)>>3);
  my $binary = substr(unpack("B*",$bytes),0,$n);
  return Math::BigInt->new("0b$binary");
}
sub urandomm {
  my($n) = @_;
  # _validate_positive_integer($n);
  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::urandomm($n))
    if $Math::Prime::Util::_GMPfunc{"urandomm"};
  return 0 if $n <= 1;
  my $r;
  if ($n <= 4294967295) {
    my $rmax = int(4294967295 / $n) * $n;
    do { $r = Math::Prime::Util::irand() } while $r >= $rmax;
  } elsif (!ref($n)) {
    my $rmax = int(~0 / $n) * $n;
    do { $r = Math::Prime::Util::irand64() } while $r >= $rmax;
  } else {
    # TODO: verify and try to optimize this
    my $bits = length($n->as_bin) - 2;
    my $bytes = 1 + (($bits+7)>>3);
    my $rmax = Math::BigInt->bone->blsft($bytes*8)->bdec;
    my $overflow = $rmax - ($rmax % $n);
    do { $r = Math::Prime::Util::urandomb($bytes*8); } while $r >= $overflow;
  }
  return $r % $n;
}

sub random_prime {
  my($low, $high) = @_;
  if (scalar(@_) == 1) { ($low,$high) = (2,$low);          }
  else                 { _validate_positive_integer($low); }
  _validate_positive_integer($high);

  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::random_prime($low, $high))
    if $Math::Prime::Util::_GMPfunc{"random_prime"};

  require Math::Prime::Util::RandomPrimes;
  return Math::Prime::Util::RandomPrimes::random_prime($low,$high);
}

sub random_ndigit_prime {
  my($digits) = @_;
  _validate_positive_integer($digits, 1);
  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::random_ndigit_prime($digits))
    if $Math::Prime::Util::_GMPfunc{"random_ndigit_prime"};
  require Math::Prime::Util::RandomPrimes;
  return Math::Prime::Util::RandomPrimes::random_ndigit_prime($digits);
}
sub random_nbit_prime {
  my($bits) = @_;
  _validate_positive_integer($bits, 2);
  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::random_nbit_prime($bits))
    if $Math::Prime::Util::_GMPfunc{"random_nbit_prime"};
  require Math::Prime::Util::RandomPrimes;
  return Math::Prime::Util::RandomPrimes::random_nbit_prime($bits);
}
sub random_strong_prime {
  my($bits) = @_;
  _validate_positive_integer($bits, 128);
  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::random_strong_prime($bits))
    if $Math::Prime::Util::_GMPfunc{"random_strong_prime"};
  require Math::Prime::Util::RandomPrimes;
  return Math::Prime::Util::RandomPrimes::random_strong_prime($bits);
}

sub random_maurer_prime {
  my($bits) = @_;
  _validate_positive_integer($bits, 2);

  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::random_maurer_prime($bits))
    if $Math::Prime::Util::_GMPfunc{"random_maurer_prime"};

  require Math::Prime::Util::RandomPrimes;
  my ($n, $cert) = Math::Prime::Util::RandomPrimes::random_maurer_prime_with_cert($bits);
  croak "maurer prime $n failed certificate verification!"
        unless Math::Prime::Util::verify_prime($cert);

  return $n;
}

sub random_shawe_taylor_prime {
  my($bits) = @_;
  _validate_positive_integer($bits, 2);

  return Math::Prime::Util::_reftyped($_[0], Math::Prime::Util::GMP::random_shawe_taylor_prime($bits))
    if $Math::Prime::Util::_GMPfunc{"random_shawe_taylor_prime"};

  require Math::Prime::Util::RandomPrimes;
  my ($n, $cert) = Math::Prime::Util::RandomPrimes::random_shawe_taylor_prime_with_cert($bits);
  croak "shawe-taylor prime $n failed certificate verification!"
        unless Math::Prime::Util::verify_prime($cert);

  return $n;
}

sub miller_rabin_random {
  my($n, $k, $seed) = @_;
  _validate_positive_integer($n);
  if (scalar(@_) == 1 ) { $k = 1; } else { _validate_positive_integer($k); }

  return 1 if $k <= 0;

  if ($Math::Prime::Util::_GMPfunc{"miller_rabin_random"}) {
    return Math::Prime::Util::GMP::miller_rabin_random($n, $k, $seed) if defined $seed;
    return Math::Prime::Util::GMP::miller_rabin_random($n, $k);
  }

  # Math::Prime::Util::prime_get_config()->{'assume_rh'})  ==>  2*log(n)^2
  if ($k >= int(3*$n/4) ) {
    for (2 .. int(3*$n/4)+2) {
      return 0 unless Math::Prime::Util::is_strong_pseudoprime($n, $_);
    }
    return 1;
  }
  my $brange = $n-2;
  return 0 unless Math::Prime::Util::is_strong_pseudoprime($n, Math::Prime::Util::urandomm($brange)+2 );
  $k--;
  while ($k > 0) {
    my $nbases = ($k >= 20) ? 20 : $k;
    return 0 unless is_strong_pseudoprime($n, map { urandomm($brange)+2 } 1 .. $nbases);
    $k -= $nbases;
  }
  1;
}

sub random_semiprime {
  my($b) = @_;
  return 0 if defined $b && int($b) < 0;
  _validate_positive_integer($b,4);

  my $n;
  my $min = ($b <= MPU_MAXBITS)  ?  (1 << ($b-1))  :  BTWO->copy->bpow($b-1);
  my $max = $min + ($min - 1);
  my $L = $b >> 1;
  my $N = $b - $L;
  my $one = ($b <= MPU_MAXBITS) ? 1 : BONE;
  do {
    $n = $one * random_nbit_prime($L) * random_nbit_prime($N);
  } while $n < $min || $n > $max;
  $n = _bigint_to_int($n) if ref($n) && $n->bacmp(BMAX) <= 0;
  $n;
}

sub random_unrestricted_semiprime {
  my($b) = @_;
  return 0 if defined $b && int($b) < 0;
  _validate_positive_integer($b,3);

  my $n;
  my $min = ($b <= MPU_MAXBITS)  ?  (1 << ($b-1))  :  BTWO->copy->bpow($b-1);
  my $max = $min + ($min - 1);

  if ($b <= 64) {
    do {
      $n = $min + urandomb($b-1);
    } while !Math::Prime::Util::is_semiprime($n);
  } else {
    # Try to get probabilities right for small divisors
    my %M = (
      2 => 1.91218397452243,
      3 => 1.33954826555021,
      5 => 0.854756717114822,
      7 => 0.635492301836862,
      11 => 0.426616792046787,
      13 => 0.368193843118344,
      17 => 0.290512701603111,
      19 => 0.263359264658156,
      23 => 0.222406328935102,
      29 => 0.181229250520242,
      31 => 0.170874199059434,
      37 => 0.146112155735473,
      41 => 0.133427839963585,
      43 => 0.127929010905662,
      47 => 0.118254609086782,
      53 => 0.106316418106489,
      59 => 0.0966989675438643,
      61 => 0.0938833658008547,
      67 => 0.0864151823151671,
      71 => 0.0820822953188297,
      73 => 0.0800964416340746,
      79 => 0.0747060914833344,
      83 => 0.0714973706654851,
      89 => 0.0672115468436284,
      97 => 0.0622818892486191,
      101 => 0.0600855891549939,
      103 => 0.0590613570015407,
      107 => 0.0570921135626976,
      109 => 0.0561691667641485,
      113 => 0.0544330141081874,
      127 => 0.0490620204315701,
    );
    my ($p,$r);
    $r = Math::Prime::Util::drand();
    for my $prime (2..127) {
      next unless defined $M{$prime};
      my $PR = $M{$prime} / $b  +  0.19556 / $prime;
      if ($r <= $PR) {
        $p = $prime;
        last;
      }
      $r -= $PR;
    }
    if (!defined $p) {
      # Idea from Charles Greathouse IV, 2010.  The distribution is right
      # at the high level (small primes weighted more and not far off what
      # we get with the uniform selection), but there is a noticeable skew
      # toward primes with a large gap after them.  For instance 3 ends up
      # being weighted as much as 2, and 7 more than 5.
      #
      # Since we handled small divisors earlier, this is less bothersome.
      my $M = 0.26149721284764278375542683860869585905;
      my $weight = $M + log($b * log(2)/2);
      my $minr = log(log(131));
      do {
        $r  = Math::Prime::Util::drand($weight) - $M;
      } while $r < $minr;
      # Using Math::BigFloat::bexp is ungodly slow, so avoid at all costs.
      my $re = exp($r);
      my $a = ($re < log(~0)) ? int(exp($re)+0.5)
                              : _upgrade_to_float($re)->bexp->bround->as_int;
      $p = $a < 2 ? 2 : Math::Prime::Util::prev_prime($a+1);
    }
    my $ranmin = ref($min) ? $min->badd($p-1)->bdiv($p)->as_int : int(($min+$p-1)/$p);
    my $ranmax = ref($max) ? $max->bdiv($p)->as_int : int($max/$p);
    my $q = random_prime($ranmin, $ranmax);
    $n = Math::Prime::Util::vecprod($p,$q);
  }
  $n = _bigint_to_int($n) if ref($n) && $n->bacmp(BMAX) <= 0;
  $n;
}

sub random_factored_integer {
  my($n) = @_;
  return (0,[]) if defined $n && int($n) < 0;
  _validate_positive_integer($n,1);

  while (1) {
    my @S = ($n);
    # make s_i chain
    push @S, 1 + Math::Prime::Util::urandomm($S[-1])  while $S[-1] > 1;
    # first is n, last is 1
    @S = grep { is_prime($_) } @S[1 .. $#S-1];
    my $r = Math::Prime::Util::vecprod(@S);
    return ($r, [@S]) if $r <= $n && (1+urandomm($n)) <= $r;
  }
}



1;

__END__


# ABSTRACT: Pure Perl version of Math::Prime::Util

=pod

=encoding utf8


=head1 NAME

Math::Prime::Util::PP - Pure Perl version of Math::Prime::Util


=head1 VERSION

Version 0.73


=head1 SYNOPSIS

The functionality is basically identical to L<Math::Prime::Util>, as this
module is just the Pure Perl implementation.  This documentation will only
note differences.

  # Normally you would just import the functions you are using.
  # Nothing is exported by default.
  use Math::Prime::Util ':all';


=head1 DESCRIPTION

Pure Perl implementations of prime number utilities that are normally
handled with XS or GMP.  Having the Perl implementations (1) provides examples,
(2) allows the functions to run even if XS isn't available, and (3) gives
big number support if L<Math::Prime::Util::GMP> isn't available.  This is a
subset of L<Math::Prime::Util>'s functionality.

All routines should work with native integers or multi-precision numbers.  To
enable big numbers, use bigint or bignum:

    use bigint;
    say prime_count_approx(1000000000000000000000000)'
    # says 18435599767347543283712

This is still experimental, and some functions will be very slow.  The
L<Math::Prime::Util::GMP> module has much faster versions of many of these
functions.  Alternately, L<Math::Pari> has a lot of these types of functions.


=head1 FUNCTIONS

=head2 euler_phi

Takes a I<single> integer input and returns the Euler totient.

=head2 euler_phi_range

Takes two values defining a range C<low> to C<high> and returns an array
with the totient of each value in the range, inclusive.

=head2 moebius

Takes a I<single> integer input and returns the Moebius function.

=head2 moebius_range

Takes two values defining a range C<low> to C<high> and returns an array
with the Moebius function of each value in the range, inclusive.


=head1 LIMITATIONS

The SQUFOF and Fermat factoring algorithms are not implemented yet.

Some of the prime methods use more memory than they should, as the segmented
sieve is not properly used in C<primes> and C<prime_count>.


=head1 PERFORMANCE

Performance compared to the XS/C code is quite poor for many operations.  Some
operations that are relatively close for small and medium-size values:

  next_prime / prev_prime
  is_prime / is_prob_prime
  is_strong_pseudoprime
  ExponentialIntegral / LogarithmicIntegral / RiemannR
  primearray

Operations that are slower include:

  primes
  random_prime / random_ndigit_prime
  factor / factor_exp / divisors
  nth_prime
  prime_count
  is_aks_prime

Performance improvement in this code is still possible.  The prime sieve is
over 2x faster than anything I was able to find online, but it is still has
room for improvement.

L<Math::Prime::Util::GMP> offers C<C+XS+GMP> support for most of the important
functions, and will be vastly faster for most operations.  If you install that
module, L<Math::Prime::Util> will load it automatically, meaning you should
not have to think about what code is actually being used (C, GMP, or Perl).

Memory use will generally be higher for the PP code, and in some cases B<much>
higher.  Some of this may be addressed in a later release.

For small values (e.g. primes and prime counts under 10M) most of this will
not matter.


=head1 SEE ALSO

L<Math::Prime::Util>

L<Math::Prime::Util::GMP>


=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>


=head1 COPYRIGHT

Copyright 2012-2016 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
