#!perl
use strict;
use warnings;
use Getopt::Long;
use Math::BigInt try => 'GMP';
use Math::Prime::Util qw/primes  prime_count  next_prime  prev_prime
                         twin_primes  sieve_prime_cluster  mulmod  is_pillai
                         is_prime  is_provable_prime  is_mersenne_prime
                         lucasu  lucasv
                         nth_prime  prime_count  primorial  pn_primorial/;
$| = 1;

# For many more types, see:
#   http://en.wikipedia.org/wiki/List_of_prime_numbers
#   http://mathworld.wolfram.com/IntegerSequencePrimes.html


# This program shouldn't contain any special knowledge about the series
# members other than perhaps the start.  It can know patterns, but don't
# include a static list of the members, for instance.  It should actually
# compute the entries in a range (though go ahead and be clever about it).
# Example:
#   DO     use knowledge that F_k is prime only if k <= 4 or k is prime.
#   DO     use knowledge that safe primes are <= 7 or congruent to 11 mod 12.
#   DO NOT use knowledge that fibprime(14) = 19134702400093278081449423917


# The various primorial primes are confusing.  Some things to consider:
#   1) there are two definitions of primorial: p# and p_n#
#   2) three sequences:
#         p where 1+p# is prime
#         n where 1+p_n# is prime
#         p_n#+1 where 1+p_n# is prime
#   3) intersections of sequences (e.g. p_n#+1 and p_n#-1)
#   4) other sequences like A057705: p where p+1 is an A002110 primorial
#      plus all the crazy primorial sequences (unlikely to be confused)
#
# A005234  p      where p#+1   prime
# A136351  p#     where p#+1   prime     2,6,30,210,2310,200560490130
# A014545  n      where p_n#+1 prime     1,2,3,4,5,11,75,171,172
# A018239  p_n#+1 where p_n#+1 prime
#
# A006794  p      where p#-1   prime     3,5,11,13,41,89,317,337
# A057704  n      where p_n#-1 prime     2,3,5,6,13,24,66,68,167
#
# As an aside, the 18th p#-1 is 15877, but the 19th is 843301.
# The p#+1's are a bit denser, with the 22nd at 392113.


# There are a few of these prime filters that Math::NumSeq supports, and in
# theory it will add them eventually since they are OEIS sequences.  Many are
# of the form "primes from ####" so aren't hard to work up.  Math::NumSeq is
# a really neat module for playing with OEIS sequences.
#
# Example: All Sophie Germain primes under 1M
#    primes.pl --sophie 1 1000000
#    perl -MMath::NumSeq::SophieGermainPrimes=:all -E 'my $seq = Math::NumSeq::SophieGermainPrimes->new; my $v = 0; while (1) { $v = ($seq->next)[1]; last if $v > $end; say $v; } BEGIN {our $end = 1000000}'
#
# Timing from 1 .. N for small N is going to be similar.  As N increases, the
# time difference grows rapidly.
#
#          primes.pl    Math::NumSeq::SophieGermainPrimes
#     1M     0.06s        0.13s
#    10M     0.21        2.91
#   100M     1.52      396
#  1000M    13.7       > a day
#
# If given a non-zero start value it spreads even more, as for most sequences
# primes.pl doesn't have to generate preceeding values, while NumSeq has to
# start at the beginning.  Additionally, Math::NumSeq may or may not deal with
# numbers larger than 2^32 (many sequences do, but it uses Math::Factor::XS
# for factoring and primality, which is limited to 32-bit).
#
# Here's an example of a combination.  Palindromic primes:
#    primes.pl --palin 1 1000000000
#    perl -MMath::Prime::Util=is_prime -MMath::NumSeq::Palindromes=:all -E 'my $seq = Math::NumSeq::Palindromes->new; my $v = 0; while (1) { $v = ($seq->next)[1]; last if $v > $end; say $v if is_prime($v); } BEGIN {our $end = 1000000000}'

my %opts;
# Make Getopt not capture +
Getopt::Long::Configure(qw/no_getopt_compat/);
GetOptions(\%opts,
           'safe|A005385',
           'sophie|sg|A005384',
           'twin|A001359',
           'lucas|A005479',
           'fibonacci|A005478',
           'lucky|A031157',
           'triplet|A007529',
           'quadruplet|A007530',
           'cousin|A023200',
           'sexy|A023201',
           'mersenne|A000668',
           'palindromic|palindrome|palendrome|A002385',
           'pillai|A063980',
           'good|A028388',
           'cuban1|A002407',
           'cuban2|A002648',
           'pnp1|A005234',
           'pnm1|A006794',
           'euclid|A018239',
           'circular|A068652',
           'panaitopol|A027862',
           'provable',
           'nompugmp',   # turn off MPU::GMP for debugging
           'version',
           'help',
          ) || die_usage();
Math::Prime::Util::prime_set_config(gmp=>0) if exists $opts{'nompugmp'};
if (exists $opts{'version'}) {
  my $version_str =
   "primes.pl version 1.3 using Math::Prime::Util $Math::Prime::Util::VERSION";
  $version_str .= " and MPU::GMP $Math::Prime::Util::GMP::VERSION"
    if Math::Prime::Util::prime_get_config->{'gmp'};
  $version_str .= "\nWritten by Dana Jacobsen.\n";
  die "$version_str";
}
die_usage() if exists $opts{'help'};

# Get the start and end values.  Verify they're positive integers.
@ARGV = (0,@ARGV) if @ARGV == 1;
die_usage() unless @ARGV == 2;
my ($start, $end) = @ARGV;
# Allow some expression evaluation on the input, but don't just eval it.
$end = "($start)$end" if $end =~ /^\+/;
$start =~ s/\s*$//;  $start =~ s/^\s*//;
$end   =~ s/\s*$//;  $end   =~ s/^\s*//;
$start = eval_expr($start) unless $start =~ /^\d+$/;
$end   = eval_expr($end  ) unless $end   =~ /^\d+$/;
die "$start isn't a positive integer" if $start =~ tr/0123456789//c;
die "$end isn't a positive integer"   if $end   =~ tr/0123456789//c;

# Turn start and end into bigints if they're very large.
# Fun fact:  Math::BigInt->new("1") <= 10000000000000000000  is false.  Sigh.
if ( ($start >= 2**63) || ($end >= 2**63) ) {
  $start = Math::BigInt->new("$start") unless ref($start) eq 'Math::BigInt';
  $end = Math::BigInt->new("$end") unless ref($end) eq 'Math::BigInt';
}

my $segment_size = $start - $start + 30 * 128_000;   # 128kB

# Calculate the mod 210 pre-test.  This helps with the individual filters,
# but the real benefit is that it convolves the pretests, which can speed
# up even more.
my ($min_pass, %mod_pass) = find_mod210_restriction();
# Find out if they've filtered so much nothing passes (e.g. cousin quad)
if (scalar keys %mod_pass == 0) {
  $end = $min_pass if $end > $min_pass;
}

if ($start > $end) {
  # Do nothing
} elsif (   exists $opts{'lucas'}
         || exists $opts{'fibonacci'}
         || exists $opts{'euclid'}
         || exists $opts{'lucky'}
         || exists $opts{'mersenne'}
         || exists $opts{'cuban1'}
         || exists $opts{'cuban2'}
        ) {
  my $p = gen_and_filter($start, $end);
  print join("\n", @$p), "\n"  if scalar @$p > 0;
} else {
  while ($start <= $end) {

    # Adjust segment sizes for some cases
    $segment_size = 10000 if $start > ~0;   # small if doing bigints
    if (exists $opts{'pillai'}) {
      $segment_size = ($start < 10000) ? 100 : 1000;  # very small for Pillai
    }
    if (exists $opts{'pnp1'} || exists $opts{'pnm1'}) {
      $segment_size = 500;
    }
    if (exists $opts{'palindromic'}) {
      $segment_size = 10**length($start) - $start - 1; # all n-digit numbers
    }
    if (exists $opts{'panaitopol'}) {
      $segment_size = (~0 == 4294967295) ? 2147483648 : int(10**12);
    }

    my $seg_start = $start;
    my $seg_end = int($start + $segment_size);
    $seg_end = $end if $end < $seg_end;
    $start = $seg_end+1;

    my $p = gen_and_filter($seg_start, $seg_end);

    # print this segment
    print join("\n", @$p), "\n"  if scalar @$p > 0;
  }
}


# This is OEIS A000032, Lucas numbers beginning at 2.
sub lucas_primes {
  my ($start, $end) = @_;
  my ($k, $Lk, @lprimes) = (0);
  do {
    $Lk = lucasv(1,-1,$k);
    push @lprimes, $Lk if $Lk >= $start && is_prime($Lk);
    $k++;
  } while $Lk < $end;
  @lprimes;
}

sub fibonacci_primes {
  my ($start, $end) = @_;
  my ($k, $Fk, @fprimes) = (3);
  do {
    $Fk = lucasu(1,-1,$k);
    push @fprimes, $Fk if $Fk >= $start && is_prime($Fk);
    $k = ($k <= 4)  ?  $k+1  :  next_prime($k);
  } while $Fk < $end;
  @fprimes;
}

sub mersenne_primes {
  my ($start, $end) = @_;
  my @mprimes;
  my $p = 1;
  while (1) {
    $p = next_prime($p);  # Mp is not prime if p is not prime
    next if $p > 3 && ($p % 4) == 3 && is_prime(2*$p+1);
    my $Mp = Math::BigInt->bone->blsft($p)->bdec;
    last if $Mp > $end;
    push @mprimes, $Mp if $Mp >= $start && is_mersenne_prime($p);
  }
  @mprimes;
}

sub euclid_primes {
  my ($start, $end, $add) = @_;
  my @eprimes;
  my $k = 0;
  while (1) {
    my $primorial = pn_primorial(Math::BigInt->new($k)) + $add;
    last if $primorial > $end;
    push @eprimes, $primorial if $primorial >= $start && is_prime($primorial);
    $k++;
  }
  @eprimes;
}

sub cuban_primes {
  my ($start, $end, $add) = @_;
  my @cprimes;
  my $psub = ($add == 1) ? sub { 3*$_[0]*$_[0] + 3*$_[0] + 1 }
                         : sub { 3*$_[0]*$_[0] + 6*$_[0] + 4 };
  # Determine first y via quadratic equation (under-estimate)
  my $y = ($start <= 2)  ?  0  :
          ($add == 1)
          ? int((-3 + sqrt(3*3 - 4*3*(1-$start))) / (2*3))
          : int((-6 + sqrt(6*6 - 4*3*(4-$start))) / (2*3));
  die "Incorrect start calculation" if $y > 0 && $psub->($y - 1) >= $start;

  # skip forward until p >= start
  $y++ while $psub->($y) < $start;

  my $p = $psub->($y);
  while ($p <= $end) {
    push @cprimes, $p if is_prime($p);
    $p = $psub->(++$y);
  }
  @cprimes;
}

sub panaitopol_primes {
  my ($start, $end) = @_;

  my @init;
  push @init,  5 if $start <=  5 && $end >=  5;
  push @init, 13 if $start <= 13 && $end >= 13;
  return @init if $end < 41;
  my $nbeg = ($start <= 41)  ?  4  :  int( sqrt( ($start-1)/2) );
  my $nend = int( sqrt(($end-1)/2) );
  $nbeg++ while (2*$nbeg*($nbeg+1)+1) < $start;
  $nend-- while (2*$nend*($nend+1)+1) > $end;
  # TODO: BigInts
  return @init,
         grep { is_prime($_) }
         grep { ($_%5) && ($_%13) && ($_%17) && ($_%29) && ($_%37) }
         map { 2*$_*($_+1)+1 }
         $nbeg .. $nend;
}


sub lucky_primes {
  my ($start, $end) = @_;

  # First do a lucky number sieve to generate A000959.

  my @_lf63;   # Lucky:  1,3,7,9,13,15,...  63=7*9.
  $_lf63[$_] = 1 for (qw/2 5 8 11 14 17 18 19 20 23 26 27 28 29 32 35 38 39 40 41 44 47 50 53 56 57 58 59 60 61 62/);

  my @lucky;
  my $n = 1;
  while ($n <= $end) {
    my $m63 = $n % 63;
    push @lucky, $n unless $_lf63[$m63];
    push @lucky, $n+2 unless $_lf63[$m63+2];
    $n += 6;
  }
  delete $lucky[-1] if $lucky[-1] > $end;

  for (my $k = 4; $k < scalar @lucky && $lucky[$k]-1 <= $#lucky; $k++) {
    my $skip = $lucky[$k]-1;
    my $index = $skip;
    while ($index <= $#lucky) {
      splice(@lucky, $index, 1);
      $index += $skip;
    }
  }
  shift @lucky while $lucky[0] < $start;

  # Then restrict to primes to get A031157.
  grep { is_prime($_) } @lucky;
}

# This is not a general palindromic digit function!
sub ndig_palindromes {
  my $digits = shift;
  return (2,3,5,7) if $digits == 1;
  return (11) if $digits == 2;
  return () if ($digits % 2) == 0;
  my $rhdig = int(($digits - 1) / 2);

  # return grep { is_prime($_) }
  #        map { $_ . reverse substr($_,0,$rhdig) }
  #        map { $_ * int(10**$rhdig) .. ($_+1) * int(10**$rhdig) - 1 }
  #        1, 3, 7, 9;

  my @pp;
  for my $pre (1,3,7,9) {
    my $beg = $pre     * int(10**$rhdig);
    my $end = ($pre+1) * int(10**$rhdig);
    while ($beg < $end) {
      my $c = $beg . reverse substr($beg,0,$rhdig);
      push @pp,$c if is_prime($c);
      $beg++;
    }
  }
  return @pp;
}

# Not fast.
sub is_good_prime {
  my $p = shift;
  return 0 if $p <= 2; # 2 isn't a good prime
  my $lower = $p;
  my $upper = $p;
  while ($lower > 2) {
    $lower = prev_prime($lower);
    $upper = next_prime($upper);
    return 0 if ($p*$p) <= ($upper * $lower);
  }
  1;
}

# Assumes the input is prime.  Returns 1 if all digit rotations are prime.
sub is_circular_prime {
  my $p = shift;
  return 1 if $p < 10;
  return 0 if $p =~ tr/024568//;
  # TODO: BigInts
  foreach my $rot (1 .. length($p)-1) {
    return 0 unless is_prime( substr($p, $rot) . substr($p, 0, $rot) );
  }
  1;
}

sub merge_primes {
  my ($genref, $pref, $name, @primes) = @_;
  if (!defined $$genref) {
    @$pref = @primes;
    $$genref = $name;
  } else {
    my %f;
    undef @f{ @primes };
    @$pref = grep { exists $f{$_} } @$pref;
  }
}

# This is used for things that can generate a filtered list faster than
# searching through all primes in the range.
sub gen_and_filter {
  my ($start, $end) = @_;
  my $gen;
  my $p = [];
  $end-- if ($end % 2) == 0 && $end > 2;

  if (exists $opts{'lucas'}) {
    merge_primes(\$gen, $p, 'lucas', lucas_primes($start, $end));
  }
  if (exists $opts{'fibonacci'}) {
    merge_primes(\$gen, $p, 'fibonacci', fibonacci_primes($start, $end));
  }
  if (exists $opts{'mersenne'}) {
    merge_primes(\$gen, $p, 'mersenne', mersenne_primes($start, $end));
  }
  if (exists $opts{'euclid'}) {
    merge_primes(\$gen, $p, 'euclid', euclid_primes($start, $end, 1));
  }
  if (exists $opts{'lucky'}) {
    merge_primes(\$gen, $p, 'lucky', lucky_primes($start, $end));
  }
  if (exists $opts{'cuban1'}) {
    merge_primes(\$gen, $p, 'cuban1', cuban_primes($start, $end, 1));
  }
  if (exists $opts{'cuban2'}) {
    merge_primes(\$gen, $p, 'cuban2', cuban_primes($start, $end, 2));
  }
  if (exists $opts{'panaitopol'}) {
    merge_primes(\$gen, $p, 'panaitopol', panaitopol_primes($start, $end));
  }
  if (exists $opts{'palindromic'}) {
    if (!defined $gen) {
      foreach my $d (length($start) .. length($end)) {
        push @$p, grep { $_ >= $start && $_ <= $end }
                  ndig_palindromes($d);
      }
      $gen = 'palindromic';
    } else {
      @$p = grep { $_ eq reverse $_; } @$p;
    }
  }

  # Combine the cluster types and use an efficient cluster sieve if possible
  if (!defined $gen) {
    my @cluster;
    if (defined $opts{'twin'})       { $cluster[2] = 1; }
    if (defined $opts{'cousin'})     { $cluster[4] = 1; }
    if (defined $opts{'sexy'})       { $cluster[6] = 1; }
    if (defined $opts{'triplet'})    { $cluster[6] = 1; }
    if (defined $opts{'quadruplet'}) { $cluster[$_] = 1 for (2,6,8); }
    @cluster = grep { defined $cluster[$_] } 0 .. $#cluster;
    if (scalar @cluster) {
      if (scalar(@cluster) == 1 && $cluster[0] == 2) {
        $p = twin_primes($start, $end);
      } else {
        $p = [sieve_prime_cluster($start, $end, @cluster)];
      }
      $gen = 'cluster';
    }
  }

  if (!defined $gen) {
    $p = primes($start, $end);
    $gen = 'primes';
  }

  # Apply the mod 210 pretest
  if ($min_pass > 0) {
    @$p = grep { $_ <= $min_pass || exists $mod_pass{$_ % 210} } @$p;
  }

  # If we didn't generate the list with a cluster sieve, grep them out
  if ($gen ne 'cluster') {
    if (exists $opts{'twin'}) {
      @$p = grep { is_prime( $_+2 ); } @$p;
    }
    if (exists $opts{'quadruplet'}) {
      @$p = grep { is_prime($_+2) && is_prime($_+6) && is_prime($_+8); } @$p;
    }
    if (exists $opts{'triplet'}) {
      @$p = grep { is_prime($_+6) && (is_prime($_+2) || is_prime($_+4)); } @$p;
    }
    if (exists $opts{'cousin'}) {
      @$p = grep { is_prime($_+4); } @$p;
    }
    if (exists $opts{'sexy'}) {
      @$p = grep { is_prime($_+6); } @$p;
    }
  } else {
    # Cluster sieve for triplet gives us just p+6.
    if (exists $opts{'triplet'} && !exists $opts{'twin'} && !exists $opts{'cousin'} && !exists $opts{'quadruplet'}) {
      @$p = grep { is_prime($_+2) || is_prime($_+4); } @$p;
    }
  }

  if (exists $opts{'safe'}) {
    @$p = grep { is_prime( ($_-1) >> 1 ); }
          grep { ($_ <= 7) || ($_ % 12) == 11; }
          @$p;
  }
  if (exists $opts{'sophie'}) {
    @$p = grep { is_prime( 2*$_+1 ); } @$p;
  }
  #if (exists $opts{'cuban1'}) {
  #  @p = grep { my $n = sqrt((4*$_-1)/3); 4*$_ == int($n)*int($n)*3+1; } @p;
  #}
  #if (exists $opts{'cuban2'}) {
  #  @p = grep { my $n = sqrt(($_-1)/3); $_ == int($n)*int($n)*3+1; } @p;
  #}
  if (exists $opts{'pnm1'}) {
    @$p = grep { is_prime( primorial(Math::BigInt->new($_))-1 ) } @$p;
  }
  if (exists $opts{'pnp1'}) {
    @$p = grep { is_prime( primorial(Math::BigInt->new($_))+1 ) } @$p;
  }
  if (exists $opts{'circular'}) {
    @$p = grep { is_circular_prime($_) } @$p;
  }
  if (exists $opts{'pillai'}) {
  # See: http://en.wikipedia.org/wiki/Pillai_prime
    @$p = grep { is_pillai($_); } @$p;
  }
  if (exists $opts{'good'}) {
    @$p = grep { is_good_prime($_); } @$p;
  }
  if (exists $opts{'provable'}) {
    @$p = grep { is_provable_prime($_) == 2; } @$p;
  }
  $p;
}

{
  my %_mod210_restrict = (
    cuban1     => {min=> 7, mod=>[1,19,37,61,79,121,127,169,187]},
    cuban2     => {min=> 2, mod=>[1,13,43,109,139,151,169,181,193]},
    twin       => {min=> 5, mod=>[11,17,29,41,59,71,101,107,137,149,167,179,191,197,209]},
    triplet    => {min=> 7, mod=>[11,13,17,37,41,67,97,101,103,107,137,163,167,187,191,193]},
    quadruplet => {min=> 5, mod=>[11,101,191]},
    cousin     => {min=> 7, mod=>[13,19,37,43,67,79,97,103,109,127,139,163,169,187,193]},
    sexy       => {min=> 7, mod=>[11,13,17,23,31,37,41,47,53,61,67,73,83,97,101,103,107,121,131,137,143,151,157,163,167,173,181,187,191,193]},
    safe       => {min=>11, mod=>[17,23,47,53,59,83,89,107,137,143,149,167,173,179,209]},
    sophie     => {min=> 5, mod=>[11,23,29,41,53,71,83,89,113,131,149,173,179,191,209]},
    panaitopol => {min=> 5, mod=>[1,11,13,41,43,53,61,71,83,103,113,131,151,173,181,193]},
    # Nothing for good, pillai, palindromic, fib, lucas, mersenne, primorials
  );

  sub find_mod210_restriction {
    my %mods_left;
    undef @mods_left{ grep { ($_%2) && ($_%3) && ($_%5) && ($_%7) } (0..209) };

    my $min = 0;
    while (my($filter,$data) = each %_mod210_restrict) {
      next unless exists $opts{$filter};
      $min = $data->{min} if $min < $data->{min};
      my %thismod;
      undef @thismod{ @{$data->{mod}} };
      foreach my $m (keys %mods_left) {
        delete $mods_left{$m} unless exists $thismod{$m};
      }
    }
    return ($min, %mods_left);
  }
}

# This is rather braindead.  We're going to eval their input so they can give
# arbitrary expressions.  But we only want to allow math-like strings.
sub eval_expr {
  my $expr = shift;
  die "$expr cannot be evaluated" if $expr =~ /:/;  # Use : for escape
  $expr =~ s/nth_prime\(/:1(/g;
  $expr =~ s/log\(/:2(/g;
  die "$expr cannot be evaluated" if $expr =~ tr|-0123456789+*/() :||c;
  $expr =~ s/:1/nth_prime/g;
  $expr =~ s/:2/log/g;
  $expr =~ s/(\d+)/ Math::BigInt->new($1) /g;
  my $res = eval $expr; ## no critic
  die "Cannot eval: $expr\n" if !defined $res;
  $res = int($res->bstr) if ref($res) eq 'Math::BigInt' && $res <= ~0;
  $res;
}


sub die_usage {
  die <<EOU;
Usage: $0 [options]  [START]  END

Displays all primes between the positive integers START and END, inclusive.
The START and END values must be integers or simple expressions.  This allows
inputs like "10**500+100" or "2**64-1000" or "2 * nth_prime(560)".
Additionally, if END starts with '+' then it is assumed to add to START.
If only one number is given, primes up to that number are shown (START = 0).

General options:

  --help       displays this help message

Filter options, which will cause the list of primes to be further filtered
to only those primes additionally meeting these conditions:

  --twin       Twin             p+2 is prime
  --triplet    Triplet          p+6 and (p+2 or p+4) are prime
  --quadruplet Quadruplet       p+2, p+6, and p+8 are prime
  --cousin     Cousin           p+4 is prime
  --sexy       Sexy             p+6 is prime
  --safe       Safe             (p-1)/2 is also prime
  --sophie     Sophie Germain   2p+1 is also prime
  --lucas      Lucas            L_p is prime
  --fibonacci  Fibonacci        F_p is prime
  --mersenne   Mersenne         M_p = 2^p-1 is prime
  --lucky      Lucky            p is a lucky number
  --palindr    Palindromic      p is equal to p with its base-10 digits reversed
  --pillai     Pillai           n! % p = p-1 and p % n != 1 for some n
  --good       Good             p_n^2 > p_{n-i}*p_{n+i} for all i in (1..n-1)
  --cuban1     Cuban (y+1)      p = (x^3 - y^3)/(x-y), x=y+1
  --cuban2     Cuban (y+2)      p = (x^3 - y^3)/(x-y), x=y+2
  --pnp1       Primorial+1      p#+1 is prime
  --pnm1       Primorial-1      p#-1 is prime
  --euclid     Euclid           pn#+1 is prime
  --circular   Circular         all digit rotations of p are prime
  --panaitopol Panaitopol       p = (x^4-y^4)/(x^3+y^3) for some x,y
  --provable                    Ensure all primes are provably prime

Note that options can be combined, e.g. display only safe twin primes.
In all cases involving multiples (twin, triplet, etc.), the value returned
is p -- the least value of the set.

EOU
}
