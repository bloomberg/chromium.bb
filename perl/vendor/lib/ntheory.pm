package ntheory;
use strict;
use warnings;

BEGIN {
  $ntheory::AUTHORITY = 'cpan:DANAJ';
  $ntheory::VERSION = '0.73';
}

BEGIN {
  require Math::Prime::Util;
  *ntheory:: = *Math::Prime::Util::;
}

1;

__END__


# ABSTRACT: Number theory utilities

=pod

=encoding utf8

=for stopwords ntheory

=head1 NAME

ntheory - Number theory utilities

=head1 SEE

See L<Math::Prime::Util> for complete documentation.

=head1 QUICK REFERENCE

Tags:
  :all         to import almost all functions
  :rand        to import rand, srand, irand, irand64

=head2 PRIMALITY

  is_prob_prime(n)                    primality test (BPSW)
  is_prime(n)                         primality test (BPSW + extra)
  is_provable_prime(n)                primality test with proof
  is_provable_prime_with_cert(n)      primality test: (isprime,cert)
  prime_certificate(n)                as above with just certificate
  verify_prime(cert)                  verify a primality certificate
  is_mersenne_prime(p)                is 2^p-1 prime or composite
  is_aks_prime(n)                     AKS deterministic test (slow)
  is_ramanujan_prime(n)               is n a Ramanujan prime

=head2 PROBABLE PRIME TESTS

  is_pseudoprime(n,bases)                  Fermat probable prime test
  is_euler_pseudoprime(n,bases)            Euler test to bases
  is_euler_plumb_pseudoprime(n)            Euler Criterion test
  is_strong_pseudoprime(n,bases)           Miller-Rabin test to bases
  is_lucas_pseudoprime(n)                  Lucas test
  is_strong_lucas_pseudoprime(n)           strong Lucas test
  is_almost_extra_strong_lucas_pseudoprime(n, [incr])   AES Lucas test
  is_extra_strong_lucas_pseudoprime(n)     extra strong Lucas test
  is_frobenius_pseudoprime(n, [a,b])       Frobenius quadratic test
  is_frobenius_underwood_pseudoprime(n)    combined PSP and Lucas
  is_frobenius_khashin_pseudoprime(n)      Khashin's 2013 Frobenius test
  is_perrin_pseudoprime(n [,r])            Perrin test
  is_catalan_pseudoprime(n)                Catalan test
  is_bpsw_prime(n)                         combined SPSP-2 and ES Lucas
  miller_rabin_random(n, ntests)           perform random-base MR tests

=head2 PRIMES

  primes([start,] end)                array ref of primes
  twin_primes([start,] end)           array ref of twin primes
  semi_primes([start,] end)           array ref of semiprimes
  ramanujan_primes([start,] end)      array ref of Ramanujan primes
  sieve_prime_cluster(start, end, @C) list of prime k-tuples
  sieve_range(n, width, depth)        sieve out small factors to depth
  next_prime(n)                       next prime > n
  prev_prime(n)                       previous prime < n
  prime_count(n)                      count of primes <= n
  prime_count(start, end)             count of primes in range
  prime_count_lower(n)                fast lower bound for prime count
  prime_count_upper(n)                fast upper bound for prime count
  prime_count_approx(n)               fast approximate count of primes
  nth_prime(n)                        the nth prime (n=1 returns 2)
  nth_prime_lower(n)                  fast lower bound for nth prime
  nth_prime_upper(n)                  fast upper bound for nth prime
  nth_prime_approx(n)                 fast approximate nth prime
  twin_prime_count(n)                 count of twin primes <= n
  twin_prime_count(start, end)        count of twin primes in range
  twin_prime_count_approx(n)          fast approx count of twin primes
  nth_twin_prime(n)                   the nth twin prime (n=1 returns 3)
  nth_twin_prime_approx(n)            fast approximate nth twin prime
  semiprime_count(n)                  count of semiprimes <= n
  semiprime_count(start, end)         count of semiprimes in range
  semiprime_count_approx(n)           fast approximate count of semiprimes
  nth_semiprime(n)                    the nth semiprime
  nth_semiprime_approx(n)             fast approximate nth semiprime
  ramanujan_prime_count(n)            count of Ramanujan primes <= n
  ramanujan_prime_count(start, end)   count of Ramanujan primes in range
  ramanujan_prime_count_lower(n)      fast lower bound for Ramanujan count
  ramanujan_prime_count_upper(n)      fast upper bound for Ramanujan count
  ramanujan_prime_count_approx(n)     fast approximate Ramanujan count
  nth_ramanujan_prime(n)              the nth Ramanujan prime (Rn)
  nth_ramanujan_prime_lower(n)        fast lower bound for Rn
  nth_ramanujan_prime_upper(n)        fast upper bound for Rn
  nth_ramanujan_prime_approx(n)       fast approximate Rn
  legendre_phi(n,a)                   # below n not div by first a primes
  inverse_li(n)                       integer inverse logarithmic integral
  prime_precalc(n)                    precalculate primes to n
  sum_primes([start,] end)            return summation of primes in range
  print_primes(start,end[,fd])        print primes to stdout or fd

=head2 FACTORING

  factor(n)                           array of prime factors of n
  factor_exp(n)                       array of [p,k] factors p^k
  divisors(n)                         array of divisors of n
  divisor_sum(n)                      sum of divisors
  divisor_sum(n,k)                    sum of k-th power of divisors
  divisor_sum(n,sub{...})             sum of code run for each divisor
  znlog(a, g, p)                      solve k in a = g^k mod p

=head2 ITERATORS

  forprimes { ... } [start,] end      loop over primes in range
  forcomposites { ... } [start,] end  loop over composites in range
  foroddcomposites {...} [start,] end loop over odd composites in range
  forsemiprimes {...} [start,] end    loop over semiprimes in range
  forfactored {...} [start,] end      loop with factors
  forsquarefree {...} [start,] end    loop with factors of square-free n
  fordivisors { ... } n               loop over the divisors of n
  forpart { ... } n [,{...}]          loop over integer partitions
  forcomp { ... } n [,{...}]          loop over integer compositions
  forcomb { ... } n, k                loop over combinations
  forperm { ... } n                   loop over permutations
  formultiperm { ... } \@n            loop over multiset permutations
  forderange { ... } n                loop over derangements
  forsetproduct { ... } \@a[,...]     loop over Cartesian product of lists
  prime_iterator                      returns a simple prime iterator
  prime_iterator_object               returns a prime iterator object
  lastfor                             stop iteration of for.... loop

=head2 RANDOM NUMBERS

  irand                               random 32-bit integer
  irand64                             random 64-bit integer
  drand([limit])                      random NV in [0,1) or [0,limit)
  random_bytes(n)                     string with n random bytes
  entropy_bytes(n)                    string with n entropy-source bytes
  urandomb(n)                         random integer less than 2^n
  urandomm(n)                         random integer less than n
  csrand(data)                        seed the CSPRNG with binary data
  srand([seed])                       simple seed (exported with :rand)
  rand([limit])                       alias for drand (exported with :rand)
  random_factored_integer(n)          random [1..n] and array ref of factors

=head2 RANDOM PRIMES

  random_prime([start,] end)          random prime in a range
  random_ndigit_prime(n)              random prime with n digits
  random_nbit_prime(n)                random prime with n bits
  random_strong_prime(n)              random strong prime with n bits
  random_proven_prime(n)              random n-bit prime with proof
  random_proven_prime_with_cert(n)    as above and include certificate
  random_maurer_prime(n)              random n-bit prime w/ Maurer's alg.
  random_maurer_prime_with_cert(n)    as above and include certificate
  random_shawe_taylor_prime(n)        random n-bit prime with S-T alg.
  random_shawe_taylor_prime_with_cert(n) as above including certificate
  random_unrestricted_semiprime(n)    random n-bit semiprime
  random_semiprime(n)                 as above with equal size factors

=head2 LISTS

  vecsum(@list)                       integer sum of list
  vecprod(@list)                      integer product of list
  vecmin(@list)                       minimum of list of integers
  vecmax(@list)                       maximum of list of integers
  vecextract(\@list, mask)            select from list based on mask
  vecreduce { ... } @list             reduce / left fold applied to list
  vecall { ... } @list                return true if all are true
  vecany { ... } @list                return true if any are true
  vecnone { ... } @list               return true if none are true
  vecnotall { ... } @list             return true if not all are true
  vecfirst { ... } @list              return first value that evals true
  vecfirstidx { ... } @list           return first index that evals true

=head2 MATH

  todigits(n[,base[,len]])            convert n to digit array in base
  todigitstring(n[,base[,len]])       convert n to string in base
  fromdigits(\@d,[,base])             convert base digit vector to number
  fromdigits(str,[,base])             convert base digit string to number
  sumdigits(n)                        sum of digits, with optional base
  is_square(n)                        return 1 if n is a perfect square
  is_power(n)                         return k if n = c^k for integer c
  is_power(n,k)                       return 1 if n = c^k for integer c, k
  is_power(n,k,\$root)                as above but also set $root to c.
  is_prime_power(n)                   return k if n = p^k for prime p
  is_prime_power(n,\$p)               as above but also set $p to p
  is_square_free(n)                   return true if no repeated factors
  is_carmichael(n)                    is n a Carmichael number
  is_quasi_carmichael(n)              is n a quasi-Carmichael number
  is_primitive_root(r,n)              is r a primitive root mod n
  is_pillai(n)                        v where  v! % n == n-1  and  n % v != 1
  is_semiprime(n)                     does n have exactly 2 prime factors
  is_polygonal(n,k)                   is n a k-polygonal number
  is_polygonal(n,k,\$root)            as above but also set $root
  is_fundamental(d)                   is d a fundamental discriminant
  is_totient(n)                       is n = euler_phi(x) for some x
  sqrtint(n)                          integer square root
  rootint(n,k)                        integer k-th root
  rootint(n,k,\$rk)                   as above but also set $rk to r^k
  logint(n,b)                         integer logarithm
  logint(n,b,\$be)                    as above but also set $be to b^e.
  gcd(@list)                          greatest common divisor
  lcm(@list)                          least common multiple
  gcdext(x,y)                         return (u,v,d) where u*x+v*y=d
  chinese([a,mod1],[b,mod2],...)      Chinese Remainder Theorem
  primorial(n)                        product of primes below n
  pn_primorial(n)                     product of first n primes
  factorial(n)                        product of first n integers: n!
  factorialmod(n,m)                   factorial mod m
  binomial(n,k)                       binomial coefficient
  partitions(n)                       number of integer partitions
  valuation(n,k)                      number of times n is divisible by k
  hammingweight(n)                    population count (# of binary 1s)
  kronecker(a,b)                      Kronecker (Jacobi) symbol
  addmod(a,b,n)                       a + b mod n
  mulmod(a,b,n)                       a * b mod n
  divmod(a,b,n)                       a / b mod n
  powmod(a,b,n)                       a ^ b mod n
  invmod(a,n)                         inverse of a modulo n
  sqrtmod(a,n)                        modular square root
  moebius(n)                          Moebius function of n
  moebius(beg, end)                   array of Moebius in range
  mertens(n)                          sum of Moebius for 1 to n
  euler_phi(n)                        Euler totient of n
  euler_phi(beg, end)                 Euler totient for a range
  inverse_totient(n)                  image of Euler totient
  jordan_totient(n,k)                 Jordan's totient
  carmichael_lambda(n)                Carmichael's Lambda function
  ramanujan_sum(k,n)                  Ramanujan's sum
  exp_mangoldt                        exponential of Mangoldt function
  liouville(n)                        Liouville function
  znorder(a,n)                        multiplicative order of a mod n
  znprimroot(n)                       smallest primitive root
  chebyshev_theta(n)                  first Chebyshev function
  chebyshev_psi(n)                    second Chebyshev function
  hclassno(n)                         Hurwitz class number H(n) * 12
  ramanujan_tau(n)                    Ramanujan's Tau function
  consecutive_integer_lcm(n)          lcm(1 .. n)
  lucasu(P, Q, k)                     U_k for Lucas(P,Q)
  lucasv(P, Q, k)                     V_k for Lucas(P,Q)
  lucas_sequence(n, P, Q, k)          (U_k,V_k,Q_k) for Lucas(P,Q) mod n
  bernfrac(n)                         Bernoulli number as (num,den)
  bernreal(n)                         Bernoulli number as BigFloat
  harmfrac(n)                         Harmonic number as (num,den)
  harmreal(n)                         Harmonic number as BigFloat
  stirling(n,m,[type])                Stirling numbers of 1st or 2nd type
  numtoperm(n,k)                      kth lexico permutation of n elems
  permtonum([a,b,...])                permutation number of given perm
  randperm(n,[k])                     random permutation of n elems
  shuffle(...)                        random permutation of an array

=head2 NON-INTEGER MATH

  ExponentialIntegral(x)              Ei(x)
  LogarithmicIntegral(x)              li(x)
  RiemannZeta(x)                      ζ(s)-1, real-valued Riemann Zeta
  RiemannR(x)                         Riemann's R function
  LambertW(k)                         Lambert W: solve for W in k = W exp(W)
  Pi([n])                             The constant π (NV or n digits)

=head2 SUPPORT

  prime_get_config                    gets hash ref of current settings
  prime_set_config(%hash)             sets parameters
  prime_memfree                       frees any cached memory


=head1 COPYRIGHT

Copyright 2011-2018 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
