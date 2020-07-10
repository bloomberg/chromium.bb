package Math::Prime::Util::PrimalityProving;
use strict;
use warnings;
use Carp qw/carp croak confess/;
use Math::Prime::Util qw/is_prob_prime is_strong_pseudoprime
                         is_provable_prime_with_cert
                         lucas_sequence
                         factor
                         prime_get_config
                        /;

BEGIN {
  $Math::Prime::Util::PrimalityProving::AUTHORITY = 'cpan:DANAJ';
  $Math::Prime::Util::PrimalityProving::VERSION = '0.73';
}

BEGIN {
  do { require Math::BigInt;  Math::BigInt->import(try=>"GMP,Pari"); }
    unless defined $Math::BigInt::VERSION;
}

my $_smallval = Math::BigInt->new("18446744073709551615");
my $_maxint = Math::BigInt->new( (~0 > 4294967296 && $] < 5.008) ? "562949953421312" : ''.~0 );


###############################################################################
# Pure Perl proofs
###############################################################################

my @_fsublist = (
  sub { Math::Prime::Util::PP::pbrent_factor (shift,   32*1024, 1) },
  sub { Math::Prime::Util::PP::pminus1_factor(shift, 1_000_000) },
  sub { Math::Prime::Util::PP::ecm_factor    (shift,     1_000,   5_000, 15) },
  sub { Math::Prime::Util::PP::pbrent_factor (shift,  512*1024, 7) },
  sub { Math::Prime::Util::PP::pminus1_factor(shift, 4_000_000) },
  sub { Math::Prime::Util::PP::ecm_factor    (shift,    10_000,  50_000, 10) },
  sub { Math::Prime::Util::PP::pbrent_factor (shift,  512*1024, 11) },
  sub { Math::Prime::Util::PP::pminus1_factor(shift,20_000_000) },
  sub { Math::Prime::Util::PP::ecm_factor    (shift,   100_000, 800_000, 10) },
  sub { Math::Prime::Util::PP::pbrent_factor (shift, 2048*1024, 13) },
  sub { Math::Prime::Util::PP::ecm_factor    (shift, 1_000_000, 1_000_000, 20)},
  sub { Math::Prime::Util::PP::pminus1_factor(shift, 100_000_000, 500_000_000)},
);

sub _small_cert {
  my $n = shift;
  return '' unless is_prob_prime($n);
  return join "\n", "[MPU - Primality Certificate]",
                    "Version 1.0",
                    "",
                    "Proof for:",
                    "N $n",
                    "",
                    "Type Small",
                    "N $n",
                    "";
}

# For stripping off the header on certificates so they can be combined.
sub _strip_proof_header {
  my $proof = shift;
  $proof =~ s/^\[MPU - Primality Certificate\]\nVersion \S+\n+Proof for:\nN (\d+)\n+//ms;
  return $proof;
}


sub primality_proof_lucas {
  my ($n) = shift;
  my @composite = (0, '');

  # Since this can take a very long time with a composite, try some easy cuts
  return @composite if !defined $n || $n < 2;
  return (2, _small_cert($n)) if $n < 4;
  return @composite if is_strong_pseudoprime($n,2,15,325) == 0;

  my $nm1 = $n-1;
  my @factors = factor($nm1);
  { # remove duplicate factors and make a sorted array of bigints
    my %uf;
    undef @uf{@factors};
    @factors = sort {$a<=>$b} map { Math::BigInt->new("$_") } keys %uf;
  }
  my $cert = "[MPU - Primality Certificate]\nVersion 1.0\n\nProof for:\nN $n\n\n";
  $cert .= "Type Lucas\nN $n\n";
  foreach my $i (1 .. scalar @factors) {
    $cert .= "Q[$i] " . $factors[$i-1] . "\n";
  }
  for (my $a = 2; $a < $nm1; $a++) {
    my $ap = Math::BigInt->new("$a");
    # 1. a must be coprime to n
    next unless Math::BigInt::bgcd($ap, $n) == 1;
    # 2. a^(n-1) = 1 mod n.
    next unless $ap->copy->bmodpow($nm1, $n) == 1;
    # 3. a^((n-1)/f) != 1 mod n for all f.
    next if (scalar grep { $_ == 1 }
             map { $ap->copy->bmodpow(int($nm1/$_),$n); }
             @factors) > 0;
    # Verify each factor and add to proof
    my @fac_proofs;
    foreach my $f (@factors) {
      my ($isp, $fproof) = Math::Prime::Util::is_provable_prime_with_cert($f);
      if ($isp != 2) {
        carp "could not prove primality of $n.\n";
        return (1, '');
      }
      push @fac_proofs, _strip_proof_header($fproof) if $f > $_smallval;
    }
    $cert .= "A $a\n";
    foreach my $proof (@fac_proofs) {
      $cert .= "\n$proof";
    }
    return (2, $cert);
  }
  return @composite;
}

sub primality_proof_bls75 {
  my ($n) = shift;
  my @composite = (0, '');

  # Since this can take a very long time with a composite, try some easy tests
  return @composite if !defined $n || $n < 2;
  return (2, _small_cert($n)) if $n < 4;
  return @composite if ($n & 1) == 0;
  return @composite if is_strong_pseudoprime($n,2,15,325) == 0;

  require Math::Prime::Util::PP;
  $n = Math::BigInt->new("$n") unless ref($n) eq 'Math::BigInt';
  my $nm1 = $n->copy->bdec;
  my $ONE = $nm1->copy->bone;
  my $TWO = $ONE->copy->binc;
  my $A = $ONE->copy;         # factored part
  my $B = $nm1->copy;         # unfactored part
  my @factors = ($TWO);
  croak "BLS75 error: n-1 not even" unless $nm1->is_even();
  {
    while ($B->is_even) { $B->bdiv($TWO); $A->bmul($TWO); }
    my @tf;
    if ($B <= $_maxint && prime_get_config->{'xs'}) {
      @tf = Math::Prime::Util::trial_factor("$B", 20000);
      pop @tf if $tf[-1] > 20000;
    } else {
      @tf = Math::Prime::Util::PP::trial_factor($B, 5000);
      pop @tf if $tf[-1] > 5000;
    }
    foreach my $f (@tf) {
      next if $f == $factors[-1];
      push @factors, $f;
      while (($B % $f) == 0) { $B /= $f;  $A *= $f; }
    }
  }
  my @nstack;
  # nstack should only hold composites
  if ($B->is_one) {
    # Completely factored.  Nothing.
  } elsif (is_prob_prime($B)) {
    push @factors, $B;
    $A *= $B;  $B /= $B;   # completely factored already
  } else {
    push @nstack, $B;
  }
  while (@nstack) {
    my ($s,$r) = $B->copy->bdiv($A->copy->bmul($TWO));
    my $fpart = ($A+$ONE) * ($TWO*$A*$A + ($r-$ONE) * $A + $ONE);
    last if $n < $fpart;

    my $m = pop @nstack;
    # Don't use bignum if it has gotten small enough.
    $m = int($m->bstr) if ref($m) eq 'Math::BigInt' && $m <= $_maxint;
    # Try to find factors of m, using the default set of factor subs.
    my @ftry;
    foreach my $sub (@_fsublist) {
      @ftry = $sub->($m);
      last if scalar @ftry >= 2;
    }
    # If we couldn't find a factor, skip it.
    next unless scalar @ftry > 1;
    # Process each factor
    foreach my $f (@ftry) {
      croak "Invalid factoring: B=$B m=$m f=$f" if $f == 1 || $f == $m || !$B->copy->bmod($f)->is_zero;
      if (is_prob_prime($f)) {
        push @factors, $f;
        do { $B /= $f;  $A *= $f; } while $B->copy->bmod($f)->is_zero;
      } else {
        push @nstack, $f;
      }
    }
  }
  { # remove duplicate factors and make a sorted array of bigints
    my %uf = map { $_ => 1 } @factors;
    @factors = sort {$a<=>$b} map { Math::BigInt->new("$_") } keys %uf;
  }
  # Just in case:
  foreach my $f (@factors) {
    while ($B->copy->bmod($f)->is_zero) {
      $B /= $f;  $A *= $f;
    }
  }
  # Did we factor enough?
  my ($s,$r) = $B->copy->bdiv($A->copy->bmul($TWO));
  my $fpart = ($A+$ONE) * ($TWO*$A*$A + ($r-$ONE) * $A + $ONE);
  return (1,'') if $n >= $fpart;
  # Check we didn't mess up
  croak "BLS75 error: $A * $B != $nm1" unless $A*$B == $nm1;
  croak "BLS75 error: $A not even" unless $A->is_even();
  croak "BLS75 error: A and B not coprime" unless Math::BigInt::bgcd($A, $B)->is_one;

  my $rtest = $r*$r - 8*$s;
  my $rtestroot = $rtest->copy->bsqrt;
  return @composite if $s != 0 && ($rtestroot*$rtestroot) == $rtest;

  my $cert = "[MPU - Primality Certificate]\nVersion 1.0\n\nProof for:\nN $n\n\n";
  $cert .= "Type BLS5\nN $n\n";
  my $qnum = 0;
  my $atext = '';
  my @fac_proofs;
  foreach my $f (@factors) {
    my $success = 0;
    if ($qnum == 0) {
      die "BLS5 Perl proof: Internal error, first factor not 2" unless $f == 2;
    } else {
      $cert .= "Q[$qnum] $f\n";
    }
    my $nm1_div_f = $nm1 / $f;
    foreach my $a (2 .. 10000) {
      my $ap = Math::BigInt->new($a);
      next unless $ap->copy->bmodpow($nm1, $n)->is_one;
      next unless Math::BigInt::bgcd($ap->copy->bmodpow($nm1_div_f, $n)->bdec, $n)->is_one;
      $atext .= "A[$qnum] $a\n" unless $a == 2;
      $success = 1;
      last;
    }
    $qnum++;
    return @composite unless $success;
    my ($isp, $fproof) = is_provable_prime_with_cert($f);
    if ($isp != 2) {
      carp "could not prove primality of $n.\n";
      return (1, '');
    }
    push @fac_proofs, _strip_proof_header($fproof) if $f > $_smallval;
  }
  $cert .= $atext;
  $cert .= "----\n";
  foreach my $proof (@fac_proofs) {
    $cert .= "\n$proof";
  }
  return (2, $cert);
}

###############################################################################
# Convert certificates from old array format to new string format
###############################################################################

sub _convert_cert {
  my $pdata = shift;   # pdata is a ref

  return '' if scalar @$pdata == 0;
  my $n = shift @$pdata;
  if (length($n) == 1) {
    return "Type Small\nN $n\n" if $n =~ /^[2357]$/;
    return '';
  }
  $n = Math::BigInt->new("$n") if ref($n) ne 'Math::BigInt';
  return '' if $n->is_even;

  my $method = (scalar @$pdata > 0) ? shift @$pdata : 'BPSW';

  if ($method eq 'BPSW') {
    return '' if $n > $_smallval;
    return '' if is_prob_prime($n) != 2;
    return "Type Small\nN $n\n";
  }

  if ($method eq 'Pratt' || $method eq 'Lucas') {
    if (scalar @$pdata != 2 || ref($$pdata[0]) ne 'ARRAY' || ref($$pdata[1]) eq 'ARRAY') {
      carp "verify_prime: incorrect Pratt format, must have factors and a value\n";
      return '';
    }
    my @factors = @{shift @$pdata};
    my $a = shift @$pdata;
    my $cert = "Type Lucas\nN     $n\n";
    foreach my $i (0 .. $#factors) {
      my $f = (ref($factors[$i]) eq 'ARRAY') ? $factors[$i]->[0] : $factors[$i];
      $cert .= sprintf("Q[%d]  %s\n", $i+1, $f);
    }
    $cert .= "A     $a\n\n";

    foreach my $farray (@factors) {
      if (ref($farray) eq 'ARRAY') {
        $cert .= _convert_cert($farray);
      }
    }
    return $cert;
  }
  if ($method eq 'n-1') {
    if (scalar @$pdata == 3 && ref($$pdata[0]) eq 'ARRAY' && $$pdata[0]->[0] =~ /^(B|T7|Theorem\s*7)$/i) {
      croak "Unsupported BLS7 proof in conversion";
    }
    if (scalar @$pdata != 2 || ref($$pdata[0]) ne 'ARRAY' || ref($$pdata[1]) ne 'ARRAY') {
      carp "verify_prime: incorrect n-1 format, must have factors and a values\n";
      return '';
    }
    my @factors = @{shift @$pdata};
    my @as = @{shift @$pdata};
    if (scalar @factors != scalar @as) {
      carp "verify_prime: incorrect n-1 format, must have a value for each factor\n";
      return '';
    }
    # Make sure 2 is at the top
    foreach my $i (1 .. $#factors) {
      my $f = (ref($factors[$i]) eq 'ARRAY') ? $factors[$i]->[0] : $factors[$i];
      if ($f == 2) {
        my $tf = $factors[0];  $factors[0] = $factors[$i];  $factors[$i] = $tf;
        my $ta = $as[0];  $as[0] = $as[$i];  $as[$i] = $ta;
      }
    }
    return '' unless $factors[0] == 2;
    my $cert = "Type BLS5\nN     $n\n";
    foreach my $i (1 .. $#factors) {
      my $f = (ref($factors[$i]) eq 'ARRAY') ? $factors[$i]->[0] : $factors[$i];
      $cert .= sprintf("Q[%d]  %s\n", $i, $f);
    }
    foreach my $i (0 .. $#as) {
      $cert .= sprintf("A[%d]  %s\n", $i, $as[$i]) if $as[$i] != 2;
    }
    $cert .= "----\n\n";
    foreach my $farray (@factors) {
      if (ref($farray) eq 'ARRAY') {
        $cert .= _convert_cert($farray);
      }
    }
    return $cert;
  }
  if ($method eq 'ECPP' || $method eq 'AGKM') {
    if (scalar @$pdata < 1) {
      carp "verify_prime: incorrect AGKM format\n";
      return '';
    }
    my $cert = '';
    my $q = $n;
    foreach my $block (@$pdata) {
      if (ref($block) ne 'ARRAY' || scalar @$block != 6) {
        carp "verify_prime: incorrect AGKM block format\n";
        return '';
      }
      my($ni, $a, $b, $m, $qval, $P) = @$block;
      if (Math::BigInt->new("$ni") != Math::BigInt->new("$q")) {
        carp "verify_prime: incorrect AGKM block format: block n != q\n";
        return '';
      }
      $q = ref($qval) eq 'ARRAY' ? $qval->[0] : $qval;
      if (ref($P) ne 'ARRAY' || scalar @$P != 2) {
        carp "verify_prime: incorrect AGKM block point format\n";
        return '';
      }
      my ($x, $y) = @{$P};
      $cert .= "Type ECPP\nN $ni\nA $a\nB $b\nM $m\nQ $q\nX $x\nY $y\n\n";
      if (ref($qval) eq 'ARRAY') {
        $cert .= _convert_cert($qval);
      }
    }
    return $cert;
  }
  carp "verify_prime: Unknown method: '$method'.\n";
  return '';
}


sub convert_array_cert_to_string {
  my @pdata = @_;

  # Convert reference input to array
  @pdata = @{$pdata[0]} if scalar @pdata == 1 && ref($pdata[0]) eq 'ARRAY';

  return '' if scalar @pdata == 0;

  my $n = $pdata[0];
  my $header = "[MPU - Primality Certificate]\nVersion 1.0\n\nProof for:\nN $n\n\n";

  my $cert = _convert_cert(\@pdata);
  return '' if $cert eq '';
  return $header . $cert;
}


###############################################################################
# Verify certificate
###############################################################################

sub _primality_error ($) {  ## no critic qw(ProhibitSubroutinePrototypes)
  print "primality fail: $_[0]\n" if prime_get_config->{'verbose'};
  return;  # error in certificate
}

sub _pfail ($) {            ## no critic qw(ProhibitSubroutinePrototypes)
  print "primality fail: $_[0]\n" if prime_get_config->{'verbose'};
  return;  # Failed a condition
}

sub _read_vars {
  my $lines = shift;
  my $type = shift;
  my %vars = map { $_ => 1 } @_;
  my %return;
  while (scalar keys %vars) {
    my $line = shift @$lines;
    return _primality_error("end of file during type $type") unless defined $line;
    # Skip comments and blank lines
    next if $line =~ /^\s*#/ or $line =~ /^\s*$/;
    chomp($line);
    return _primality_error("Still missing values in type $type") if $line =~ /^Type /;
    if ($line =~ /^(\S+)\s+(-?\d+)/) {
      my ($var, $val) = ($1, $2);
      $var =~ tr/a-z/A-Z/;
      return _primality_error("Type $type: repeated or inappropriate var: $line") unless defined $vars{$var};
      $return{$var} = $val;
      delete $vars{$var};
    } else {
      return _primality_error("Unrecognized line: $line");
    }
  }
  # Now return them in the order given, turned into bigints.
  return map { Math::BigInt->new("$return{$_}") } @_;
}

sub _is_perfect_square {
  my($n) = @_;

  if (ref($n) eq 'Math::BigInt') {
    my $mc = int(($n & 31)->bstr);
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

# Calculate Jacobi symbol (M|N)
sub _jacobi {
  my($n, $m) = @_;
  return 0 if $m <= 0 || ($m % 2) == 0;
  my $j = 1;
  if ($n < 0) {
    $n = -$n;
    $j = -$j if ($m % 4) == 3;
  }
  # Split loop so we can reduce n/m to non-bigints after first iteration.
  if ($n != 0) {
    while (($n % 2) == 0) {
      $n >>= 1;
      $j = -$j if ($m % 8) == 3 || ($m % 8) == 5;
    }
    ($n, $m) = ($m, $n);
    $j = -$j if ($n % 4) == 3 && ($m % 4) == 3;
    $n = $n % $m;
    $n = int($n->bstr) if ref($n) eq 'Math::BigInt' && $n <= $_maxint;
    $m = int($m->bstr) if ref($m) eq 'Math::BigInt' && $m <= $_maxint;
  }
  while ($n != 0) {
    while (($n % 2) == 0) {
      $n >>= 1;
      $j = -$j if ($m % 8) == 3 || ($m % 8) == 5;
    }
    ($n, $m) = ($m, $n);
    $j = -$j if ($n % 4) == 3 && ($m % 4) == 3;
    $n = $n % $m;
  }
  return ($m == 1) ? $j : 0;
}

# Proof handlers (parse input and call verification)

sub _prove_ecpp {
  _verify_ecpp( _read_vars($_[0], 'ECPP', qw/N A B M Q X Y/) );
}
sub _prove_ecpp3 {
  _verify_ecpp3( _read_vars($_[0], 'ECPP3', qw/N S R A B T/) );
}
sub _prove_ecpp4 {
  _verify_ecpp4( _read_vars($_[0], 'ECPP4', qw/N S R J T/) );
}
sub _prove_bls15 {
  _verify_bls15( _read_vars($_[0], 'BLS15', qw/N Q LP LQ/) );
}
sub _prove_bls3 {
  _verify_bls3( _read_vars($_[0], 'BLS3', qw/N Q A/) );
}
sub _prove_pock {
  _verify_pock( _read_vars($_[0], 'POCKLINGTON', qw/N Q A/) );
}
sub _prove_small {
  _verify_small( _read_vars($_[0], 'Small', qw/N/) );
}
sub _prove_bls5 {
  my $lines = shift;
  # No good way to do this using read_vars
  my ($n, @Q, @A);
  my $index = 0;
  $Q[0] = Math::BigInt->new(2);  # 2 is implicit
  while (1) {
    my $line = shift @$lines;
    return _primality_error("end of file during type BLS5") unless defined $line;
    # Skip comments and blank lines
    next if $line =~ /^\s*#/ or $line =~ /^\s*$/;
    # Stop when we see a line starting with -.
    last if $line =~ /^-/;
    chomp($line);
    if ($line =~ /^N\s+(\d+)/) {
      return _primality_error("BLS5: N redefined") if defined $n;
      $n = Math::BigInt->new("$1");
    } elsif ($line =~ /^Q\[(\d+)\]\s+(\d+)/) {
      $index++;
      return _primality_error("BLS5: Invalid index: $1") unless $1 == $index;
      $Q[$1] = Math::BigInt->new("$2");
    } elsif ($line =~ /^A\[(\d+)\]\s+(\d+)/) {
      return _primality_error("BLS5: Invalid index: A[$1]") unless $1 >= 0 && $1 <= $index;
      $A[$1] = Math::BigInt->new("$2");
    } else {
      return _primality_error("Unrecognized line: $line");
    }
  }
  _verify_bls5($n, \@Q, \@A);
}

sub _prove_lucas {
  my $lines = shift;
  # No good way to do this using read_vars
  my ($n, @Q, $a);
  my $index = 0;
  while (1) {
    my $line = shift @$lines;
    return _primality_error("end of file during type Lucas") unless defined $line;
    # Skip comments and blank lines
    next if $line =~ /^\s*#/ or $line =~ /^\s*$/;
    chomp($line);
    if ($line =~ /^N\s+(\d+)/) {
      return _primality_error("Lucas: N redefined") if defined $n;
      $n = Math::BigInt->new("$1");
    } elsif ($line =~ /^Q\[(\d+)\]\s+(\d+)/) {
      $index++;
      return _primality_error("Lucas: Invalid index: $1") unless $1 == $index;
      $Q[$1] = Math::BigInt->new("$2");
    } elsif ($line =~ /^A\s+(\d+)/) {
      $a = Math::BigInt->new("$1");
      last;
    } else {
      return _primality_error("Unrecognized line: $line");
    }
  }
  _verify_lucas($n, \@Q, $a);
}

# Verification routines

sub _verify_ecpp {
  my ($n, $a, $b, $m, $q, $x, $y) = @_;
  return unless defined $n;
  $a %= $n if $a < 0;
  $b %= $n if $b < 0;
  return _pfail "ECPP: $n failed N > 0" unless $n > 0;
  return _pfail "ECPP: $n failed gcd(N, 6) = 1" unless Math::BigInt::bgcd($n, 6) == 1;
  return _pfail "ECPP: $n failed gcd(4*a^3 + 27*b^2, N) = 1"
    unless Math::BigInt::bgcd(4*$a*$a*$a+27*$b*$b,$n) == 1;
  return _pfail "ECPP: $n failed Y^2 = X^3 + A*X + B mod N"
    unless ($y*$y) % $n == ($x*$x*$x + $a*$x + $b) % $n;
  return _pfail "ECPP: $n failed M >= N - 2*sqrt(N) + 1" unless $m >= $n + 1 - $n->copy->bmul(4)->bsqrt();
  return _pfail "ECPP: $n failed M <= N + 2*sqrt(N) + 1" unless $m <= $n + 1 + $n->copy->bmul(4)->bsqrt();
  return _pfail "ECPP: $n failed Q > (N^(1/4)+1)^2" unless $q > $n->copy->broot(4)->badd(1)->bpow(2);
  return _pfail "ECPP: $n failed Q < N" unless $q < $n;
  return _pfail "ECPP: $n failed M != Q" unless $m != $q;
  my ($mdivq, $rem) = $m->copy->bdiv($q);
  return _pfail "ECPP: $n failed Q divides M" unless $rem == 0;

  # Now verify the elliptic curve
  my $correct_point = 0;
  if (prime_get_config->{'gmp'} && defined &Math::Prime::Util::GMP::_validate_ecpp_curve) {
    $correct_point = Math::Prime::Util::GMP::_validate_ecpp_curve($a, $b, $n, $x, $y, $m, $q);
  } else {
    if (!defined $Math::Prime::Util::ECAffinePoint::VERSION) {
      eval { require Math::Prime::Util::ECAffinePoint; 1; }
      or do { die "Cannot load Math::Prime::Util::ECAffinePoint"; };
    }
    my $ECP = Math::Prime::Util::ECAffinePoint->new($a, $b, $n, $x, $y);
    # Compute U = (m/q)P, check U != point at infinity
    $ECP->mul( $m->copy->bdiv($q)->as_int );
    if (!$ECP->is_infinity) {
      # Compute V = qU, check V = point at infinity
      $ECP->mul( $q );
      $correct_point = 1 if $ECP->is_infinity;
    }
  }
  return _pfail "ECPP: $n failed elliptic curve conditions" unless $correct_point;
  ($n, $q);
}
sub _verify_ecpp3 {
  my ($n, $s, $r, $a, $b, $t) = @_;
  return unless defined $n;
  return _pfail "ECPP3: $n failed |A| <= N/2" unless abs($a) <= $n/2;
  return _pfail "ECPP3: $n failed |B| <= N/2" unless abs($b) <= $n/2;
  return _pfail "ECPP3: $n failed T >= 0" unless $t >= 0;
  return _pfail "ECPP3: $n failed T < N" unless $t < $n;
  my $l = ($t*$t*$t + $a*$t + $b) % $n;
  _verify_ecpp( $n,
               ($a * $l*$l) % $n,
               ($b * $l*$l*$l) % $n,
               $r*$s,
               $r,
               ($t*$l) % $n,
               ($l*$l) % $n    );
}
sub _verify_ecpp4 {
  my ($n, $s, $r, $j, $t) = @_;
  return unless defined $n;
  return _pfail "ECPP4: $n failed |J| <= N/2" unless abs($j) <= $n/2;
  return _pfail "ECPP4: $n failed T >= 0" unless $t >= 0;
  return _pfail "ECPP4: $n failed T < N" unless $t < $n;
  my $a = 3 * $j * (1728 - $j);
  my $b = 2 * $j * (1728 - $j) * (1728 - $j);
  my $l = ($t*$t*$t + $a*$t + $b) % $n;
  _verify_ecpp( $n,
               ($a * $l*$l) % $n,
               ($b * $l*$l*$l) % $n,
               $r*$s,
               $r,
               ($t*$l) % $n,
               ($l*$l) % $n    );
}

sub _verify_bls15 {
  my ($n, $q, $lp, $lq) = @_;
  return unless defined $n;
  return _pfail "BLS15: $n failed Q odd" unless $q->is_odd();
  return _pfail "BLS15: $n failed Q > 2" unless $q > 2;
  my ($m, $rem) = ($n+1)->copy->bdiv($q);
  return _pfail "BLS15: $n failed Q divides N+1" unless $rem == 0;
  return _pfail "BLS15: $n failed MQ-1 = N" unless $m*$q-1 == $n;
  return _pfail "BLS15: $n failed M > 0" unless $m > 0;
  return _pfail "BLS15: $n failed 2Q-1 > sqrt(N)" unless 2*$q-1 > $n->copy->bsqrt();
  my $D = $lp*$lp - 4*$lq;
  return _pfail "BLS15: $n failed D != 0" unless $D != 0;
  return _pfail "BLS15: $n failed jacobi(D,N) = -1" unless _jacobi($D,$n) == -1;
  return _pfail "BLS15: $n failed V_{m/2} mod N != 0"
      unless (lucas_sequence($n, $lp, $lq, $m/2))[1] != 0;
  return _pfail "BLS15: $n failed V_{(N+1)/2} mod N == 0"
      unless (lucas_sequence($n, $lp, $lq, ($n+1)/2))[1] == 0;
  ($n, $q);
}

sub _verify_bls3 {
  my ($n, $q, $a) = @_;
  return unless defined $n;
  return _pfail "BLS3: $n failed Q odd" unless $q->is_odd();
  return _pfail "BLS3: $n failed Q > 2" unless $q > 2;
  my ($m, $rem) = ($n-1)->copy->bdiv($q);
  return _pfail "BLS3: $n failed Q divides N-1" unless $rem == 0;
  return _pfail "BLS3: $n failed MQ+1 = N" unless $m*$q+1 == $n;
  return _pfail "BLS3: $n failed M > 0" unless $m > 0;
  return _pfail "BLS3: $n failed 2Q+1 > sqrt(n)" unless 2*$q+1 > $n->copy->bsqrt();
  return _pfail "BLS3: $n failed A^((N-1)/2) = N-1 mod N" unless $a->copy->bmodpow(($n-1)/2, $n) == $n-1;
  return _pfail "BLS3: $n failed A^(M/2) != N-1 mod N" unless $a->copy->bmodpow($m/2,$n) != $n-1;
  ($n, $q);
}
sub _verify_pock {
  my ($n, $q, $a) = @_;
  return unless defined $n;
  my ($m, $rem) = ($n-1)->copy->bdiv($q);
  return _pfail "Pocklington: $n failed Q divides N-1" unless $rem == 0;
  return _pfail "Pocklington: $n failed M is even" unless $m->is_even();
  return _pfail "Pocklington: $n failed M > 0" unless $m > 0;
  return _pfail "Pocklington: $n failed M < Q" unless $m < $q;
  return _pfail "Pocklington: $n failed MQ+1 = N" unless $m*$q+1 == $n;
  return _pfail "Pocklington: $n failed A > 1" unless $a > 1;
  return _pfail "Pocklington: $n failed A^(N-1) mod N = 1"
    unless $a->copy->bmodpow($n-1, $n) == 1;
  return _pfail "Pocklington: $n failed gcd(A^M - 1, N) = 1"
    unless Math::BigInt::bgcd($a->copy->bmodpow($m, $n)-1, $n) == 1;
  ($n, $q);
}
sub _verify_small {
  my ($n) = @_;
  return unless defined $n;
  return _pfail "Small n $n is > 2^64\n" if $n > $_smallval;
  return _pfail "Small n $n does not pass BPSW" unless is_prob_prime($n);
  ($n);
}

sub _verify_bls5 {
  my ($n, $Qr, $Ar) = @_;
  return unless defined $n;
  my @Q = @{$Qr};
  my @A = @{$Ar};
  my $nm1 = $n - 1;
  my $F = Math::BigInt->bone;
  my $R = $nm1->copy;
  my $index = $#Q;
  foreach my $i (0 .. $index) {
    return _primality_error "BLS5: $n failed Q[$i] doesn't exist" unless defined $Q[$i];
    $A[$i] = Math::BigInt->new(2) unless defined $A[$i];
    return _pfail "BLS5: $n failed Q[$i] > 1" unless $Q[$i] > 1;
    return _pfail "BLS5: $n failed Q[$i] < N-1" unless $Q[$i] < $nm1;
    return _pfail "BLS5: $n failed A[$i] > 1" unless $A[$i] > 1;
    return _pfail "BLS5: $n failed A[$i] < N" unless $A[$i] < $n;
    return _pfail "BLS5: $n failed Q[$i] divides N-1" unless ($nm1 % $Q[$i]) == 0;
    while (($R % $Q[$i]) == 0) {
      $F *= $Q[$i];
      $R /= $Q[$i];
    }
  }
  die "BLS5: Internal error R != (N-1)/F\n" unless $R == $nm1/$F;
  return _pfail "BLS5: $n failed F is even" unless $F->is_even();
  return _pfail "BLS5: $n failed gcd(F, R) = 1\n" unless Math::BigInt::bgcd($F,$R) == 1;
  my ($s, $r) = $R->copy->bdiv(2*$F);
  my $P = ($F+1) * (2 * $F * $F + ($r-1)*$F + 1);
  return _pfail "BLS5: $n failed n < P" unless $n < $P;
  return _pfail "BLS5: $n failed s=0 OR r^2-8s not a perfect square"
    unless $s == 0 or !_is_perfect_square($r*$r - 8*$s);
  foreach my $i (0 .. $index) {
    my $a = $A[$i];
    my $q = $Q[$i];
    return _pfail "BLS5: $n failed A[i]^(N-1) mod N = 1"
      unless $a->copy->bmodpow($nm1, $n)->is_one;
    return _pfail "BLS5: $n failed gcd(A[i]^((N-1)/Q[i])-1, N) = 1"
      unless Math::BigInt::bgcd($a->copy->bmodpow($nm1/$q, $n)->bdec, $n)->is_one;
  }
  ($n, @Q);
}

sub _verify_lucas {
  my ($n, $Qr, $a) = @_;
  return unless defined $n;
  my @Q = @{$Qr};
  my $index = $#Q;
  return _pfail "Lucas: $n failed A > 1" unless $a > 1;
  return _pfail "Lucas: $n failed A < N" unless $a < $n;
  my $nm1 = $n - 1;
  my $F = Math::BigInt->bone;
  my $R = $nm1->copy;
  return _pfail "Lucas: $n failed A^(N-1) mod N = 1"
    unless $a->copy->bmodpow($nm1, $n) == 1;
  foreach my $i (1 .. $index) {
    return _primality_error "Lucas: $n failed Q[$i] doesn't exist" unless defined $Q[$i];
    return _pfail "Lucas: $n failed Q[$i] > 1" unless $Q[$i] > 1;
    return _pfail "Lucas: $n failed Q[$i] < N-1" unless $Q[$i] < $nm1;
    return _pfail "Lucas: $n failed Q[$i] divides N-1" unless ($nm1 % $Q[$i]) == 0;
    return _pfail "Lucas: $n failed A^((N-1)/Q[$i]) mod N != 1"
      unless $a->copy->bmodpow($nm1/$Q[$i], $n) != 1;
    while (($R % $Q[$i]) == 0) {
      $F *= $Q[$i];
      $R /= $Q[$i];
    }
  }
  return _pfail("Lucas: $n failed N-1 has only factors Q") unless $R == 1 && $F == $nm1;
  shift @Q;  # Remove Q[0]
  ($n, @Q);
}

sub verify_cert {
  my $cert = (@_ == 1) ? $_[0] : convert_array_cert_to_string(@_);
  $cert = convert_array_cert_to_string($cert) if ref($cert) eq 'ARRAY';
  return 0 if $cert eq '';

  my %parts;  # Map of "N is prime if Q is prime"
  my %proof_funcs = (
    ECPP        =>  \&_prove_ecpp,    # Standard ECPP proof
    ECPP3       =>  \&_prove_ecpp3,   # Primo type 3
    ECPP4       =>  \&_prove_ecpp4,   # Primo type 4
    BLS15       =>  \&_prove_bls15,   # basic n+1, includes Primo type 2
    BLS3        =>  \&_prove_bls3,    # basic n-1
    BLS5        =>  \&_prove_bls5,    # much better n-1
    SMALL       =>  \&_prove_small,   # n <= 2^64
    POCKLINGTON =>  \&_prove_pock,    # simple n-1, Primo type 1
    LUCAS       =>  \&_prove_lucas,   # n-1 completely factored
  );
  my $base = 10;
  my $cert_type = 'Unknown';
  my $N;

  my @lines = split /^/, $cert;
  my $lines = \@lines;
  while (@$lines)  {
    my $line = shift @$lines;
    next if $line =~ /^\s*#/ or $line =~ /^\s*$/;  # Skip comments / blank lines
    chomp($line);
    if ($line =~ /^\[(\S+) - Primality Certificate\]/) {
      if ($1 ne 'MPU') {
        return _primality_error "Unknown certificate type: $1";
      }
      $cert_type = $1;
      next;
    }
    if ( ($cert_type eq 'PRIMO' && $line =~ /^\[Candidate\]/) || ($cert_type eq 'MPU' && $line =~ /^Proof for:/) ) {
      return _primality_error "Certificate with multiple N values" if defined $N;
      ($N) = _read_vars($lines, 'Proof for', qw/N/);
      if (!is_prob_prime($N)) {
        _pfail "N '$N' does not look prime.";
        return 0;
      }
      next;
    }
    if ($line =~ /^Base (\d+)/) {
      $base = $1;
      return _primality_error "Only base 10 supported, sorry" unless $base == 10;
      next;
    }
    if ($line =~ /^Type (.*?)\s*$/) {
      return _primality_error("Starting type without telling me the N value!") unless defined $N;
      my $type = $1;
      $type =~ tr/a-z/A-Z/;
      error("Unknown type: $type") unless defined $proof_funcs{$type};
      my ($n, @q) = $proof_funcs{$type}->($lines);
      return 0 unless defined $n;
      $parts{$n} = [@q];
    }
  }

  return _primality_error("No N") unless defined $N;
  my @qs = ($N);
  while (@qs) {
    my $q = shift @qs;
    # Check that this q has a chain
    if (!defined $parts{$q}) {
      if ($q > $_smallval) {
        _primality_error "q value $q has no proof\n";
        return 0;
      }
      if (!is_prob_prime($q)) {
        _pfail "Small n $q does not pass BPSW";
        return 0;
      }
    } else {
      die "Internal error: Invalid parts entry" if ref($parts{$q}) ne 'ARRAY';
      # q is prime if all it's chains are prime.
      push @qs, @{$parts{$q}};
    }
  }
  1;
}

1;

__END__


# ABSTRACT:  Primality proving

=pod

=encoding utf8

=for stopwords mul

=head1 NAME

Math::Prime::Util::PrimalityProving - Primality proofs and certificates


=head1 VERSION

Version 0.73


=head1 SYNOPSIS

=head1 DESCRIPTION

Routines to support primality proofs and certificate verification.


=head1 FUNCTIONS

=head2 primality_proof_lucas

Given a positive number C<n> as input, performs a full factorization of C<n-1>,
then attempts a Lucas test on the result.  A Pratt-style certificate is
returned.  Note that if the input is composite, this will take a B<very> long
time to return.

=head2 primality_proof_bls75

Given a positive number C<n> as input, performs a partial factorization of
C<n-1>, then attempts a proof using theorem 5 of Brillhart, Lehmer, and
Selfridge's 1975 paper.  This can take a long time to return if given a
composite, though it should not be anywhere near as long as the Lucas test.

=head2 convert_array_cert_to_string

Takes as input a Perl structure certificate, used by Math::Prime::Util
from version 0.26 through 0.29, and converts it to a multi-line text
certificate starting with "[MPU - Primality Certificate]".  This is the
new format produced and processed by Math::Prime::Util, Math::Prime::Util::GMP,
and associated tools.

=head2 verify_cert

Takes a MPU primality certificate and verifies that it does prove the
primality of the number it represents (the N after the "Proof for:" line).
For backwards compatibility, if given an old-style Perl structure, it will
be converted then verified.

The return value will be C<0> (failed to verify) or C<1> (verified).
A result of C<0> does I<not> indicate the number is composite; it only
indicates the proof given is not sufficient.

If the certificate is malformed, the routine will carp a warning in addition
to returning 0.  If the C<verbose> option is set (see L</prime_set_config>)
then if the validation fails, the reason for the failure is printed in
addition to returning 0.  If the C<verbose> option is set to 2 or higher, then
a message indicating success and the certificate type is also printed.

A later release may add support for
L<Primo|http://www.ellipsa.eu/public/primo/primo.html>
certificates, as all the method verifications are coded.


=head1 SEE ALSO

L<Math::Prime::Util>

=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>


=head1 COPYRIGHT

Copyright 2012-2013 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
