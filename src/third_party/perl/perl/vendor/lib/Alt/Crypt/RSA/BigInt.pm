package Alt::Crypt::RSA::BigInt;
use strict;
use warnings;
BEGIN {
  $Alt::Crypt::RSA::BigInt::AUTHORITY = 'cpan:DANAJ';
  $Alt::Crypt::RSA::BigInt::VERSION = '0.06';
}

=pod

=head1 NAME

Alt::Crypt::RSA::BigInt - RSA public-key cryptosystem, using Math::BigInt

=head1 DESCRIPTION

This is a modification of the Crypt::RSA module to remove all use and
dependencies on Pari and Math::Pari.

This first version is intended to be a plug-in replacement for Crypt::RSA,
with no user-visible changes.  This means some issues will remain
unresolved until future versions.

Math::Pari is completely removed.  This includes the two modules:

   - Crypt::Primes   =>   Math::Prime::Util
   - Crypt::Random   =>   Math::Prime::Util

All operations are now performed using Math::BigInt, and prefer the
GMP and Pari backends.

=head1 PERFORMANCE

Performance using GMP is 3-10 times faster than Crypt::RSA 1.99.

Using Math::BigInt::Pari, it is about half the speed at signing, and
on par when verifying.

If neither GMP nor Pari are available, performance is very slow, from
10x to 200x slower.  However this is an environment where the original
code could not run.

=head1 AUTHORS

Vipul Ved Prakash wrote the original Crypt::RSA.

Dana Jacobsen did the changes to Math::BigInt.

=head1 COPYRIGHT AND LICENSE

Copyright (c) 2001 by Vipul Ved Prakash, 2012 by Dana Jacobsen.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

