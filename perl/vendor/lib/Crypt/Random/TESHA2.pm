package Crypt::Random::TESHA2;
use strict;
use warnings;
use Carp qw/croak confess carp/;
use Time::HiRes qw/gettimeofday usleep/;
use Digest::SHA qw/sha256 sha512/;
use Crypt::Random::TESHA2::Config;

BEGIN {
  $Crypt::Random::TESHA2::AUTHORITY = 'cpan:DANAJ';
  $Crypt::Random::TESHA2::VERSION = '0.01';
}

use base qw( Exporter );
our @EXPORT_OK = qw( random_bytes random_values irand rand is_strong );
our %EXPORT_TAGS = (all => [ @EXPORT_OK ]);
our @EXPORT = qw( );  # nothing by default

my $_opt_need_strong = 0;
my $_opt_weak_ok     = 0;

my $_entropy_per_raw_byte = Crypt::Random::TESHA2::Config::entropy();
# Protect against possible abuse / misconfiguration
$_entropy_per_raw_byte = 1 if $_entropy_per_raw_byte < 1;
$_entropy_per_raw_byte = 7 if $_entropy_per_raw_byte > 7;
my $_hashalg = \&sha512;
my $_pool_size = 512;
my $_nehi = 0;            # A 64-bit counter
my $_nelo = 0;
my $_pool;
my $_C;                   # bits of entropy that we think are in the pool

# Make sure SHA512 is supported, back off to 256 if not.
if (!defined sha512("test")) {
  $_pool_size = 256;
  $_hashalg = \&sha256;
}

sub import {
  my @options;

  @options = grep { $_ !~ /^[-:]?weak$/i } @_;
  $_opt_weak_ok = 1 if @options != @_;
  @_ = @options;

  @options = grep { $_ !~ /^[-:]?strong$/i } @_;
  $_opt_need_strong = 1 if @options != @_;
  @_ = @options;

  croak "TESHA2 is not a strong randomness source on this platform"
    if $_opt_need_strong && !is_strong();

  goto &Exporter::import;
}

# Returns 1 if our installtion-time entropy measurements indicated we could
# get enough entropy to make this method work on this platform.
sub is_strong {
  return ($_entropy_per_raw_byte > 1.0) ? 1 : 0;
}

# Return $n random 32-bit values
sub random_values {
  return map { unpack("L", random_bytes(4)) } 1 .. shift;
}
# Note, only 32 bits.
# TODO: Figure out a portable non-64-bit way to make 52-bit doubles.
sub rand {
  return ($_[0]||1) * (unpack("L", random_bytes(4))/4294967296.0);
}
sub irand {
  return unpack("L", random_bytes(4));
}

# This uses an entropy pool (see Encyclopedia of Cryptography and Security,
# volume 2, "Entropy Sources").  We use SHA-512 to handle a 512-bit pool.
# One this this will do is ensure the input from any one byte is nicely
# spread out among the 64 bytes of the pool.

sub random_bytes {
  my $n = shift;
  return '' unless defined $n;
  if (!defined $_pool) {
    # Initialize pool with 64 bits of entropy.
    $_C = 64;
    # Get some extra bytes at the start.
    my $nbytes = 4 + int($_C/$_entropy_per_raw_byte) + 1;
    my $S = join("", map { _get_random_byte() } 1 .. $nbytes);
    $_pool = $_hashalg->($S);

    carp "TESHA2 is not a strong randomness source on this platform"
      if !$_opt_weak_ok && !is_strong();
  }
  my $X = '';
  while (length($X) < $n) {
    my $K = 8 * $n;
    $K = $_pool_size if $K > $_pool_size;
    # Add more entropy to pool if needed.
    while ($_C < $K) {
      my $nbytes = int( ($K - $_C) / $_entropy_per_raw_byte ) + 1;
      #warn "want $K bits, pool has $_C bits.  Adding $nbytes bytes\n";
      my $S = join("", map { _get_random_byte() } 1 .. $nbytes);
      $_pool = $_hashalg->($_pool . $S);
      $_C += $_entropy_per_raw_byte * $nbytes;
      $_C = $_pool_size if $_C > $_pool_size;
    }
    # Extract K bits from the pool.
    $_C -= $K;
    my $V = $_hashalg->( 'te' . pack("LL", $_nehi, $_nelo) . $_pool );
    if ($_nelo < 4294967295) { $_nelo++; } else { $_nehi++; $_nelo = 0; }
    $X .= substr($V, 0, int($K/8));
  }
  return $X;
}

# The heart of the system, where we gather entropy from timer jitter
# (technically this is scheduler jitter).  This is a similar idea to
# timer_entropyd, TrueRand, or the old Math::TrulyRandom module.
#
# *) Cryptographically, there are numerous issues here.  This is, at best,
#    one source to feed to a well designed entropy pool system.
#
# *) The output of this function passes ENT and TestU01 Rabbit on all systems
#    I've run it on.  timer_entropyd does not, even when von Neumann debiased.
#    However, even a counter run through SHA256 will pass these tests, which
#    just indicates the stream data is uncorrelated.
#
# *) The entropy tests mentioned above are only one part.  If a system
#    returned the same sequence every time, it may pass all the tests but
#    still be a horrible generator.  That's especially not what one wants
#    from this module.
#
# *) I discovered Matt Blaze's truerand after I wrote this -- no ideas or
#    code were used.  However, I got the idea from timer_entropyd, which
#    probably in turn got the idea from truerand.  Version 2.1 (1996) of
#    Matt Blaze's truerand is _far_ more conservative than the old design
#    in Math::TrulyRandom.  First he replaces the old-school byte mixing
#    with a SHA (very similar to my solution here).  Next, he does another
#    mixing to generate the actual bytes, while the old code would use the
#    raw results.  I use a different method above, but the end result is
#    somewhat similar -- we take these raw results and stir them further.
#
# *) My tests using the raw timer data are showing 1.5 - 4 bits per xor.
#    The truerand 2.1 documentation indicates 0.67 to 1.33 bits per call,
#    then conservatively halves the number.
#
# *) Expanding on the above, assume the worst and absolutely no entropy is
#    gathered.  Then each byte will be a sha256 related to the current hires
#    time, where each byte is mixed in the pool.  We get a fine PRNG, just
#    not seeded well (from a crypto point of view, this is awful).
#
# *) For each bit, the two microsecond values are xor'd and packed into
#    32-bit words.  Eight of these are concatenated and a SHA-256 hash is
#    performed.  As long as the sum of entropy gathered from all eight xors
#    is at least 8, we're good.  The sha256 takes care of shuffling bits so
#    there aren't biases.  This generates a much better result than using
#    boolean differences like timer_entropyd (even if the differences are
#    sent through von Neumann debiasing).
#
sub _get_random_byte {
  my ($start, $t1, $t2) = gettimeofday();
  # This basically gives us a counter, so every call is unique.  We can
  # assume it adds no entropy.
  my $str = pack("LL", $start, $t1);
  # A hash we will use for a little bit of CPU processing inside the loop.
  my %dummy;
  foreach my $bit (1 .. 8) {
    # Sleep some period of time.  Differ the times so we don't hit a
    # particular beat of the scheduler.
    usleep(2+3*$bit);
    (undef, $t2) = gettimeofday();
    # xor the two.  The entropy is in the variation between what we got
    # (t2 - t1) and what we expected (2+3*bit + c).
    $str .= pack("L", $t1 ^ $t2);
    # A little hash processing to further perturb the times.
    $dummy{$str . $_}++ for 1..8;
    $t1 = $t2;
  }
  # To truly return a byte:  return substr( sha256($str), 0, 1 );
  # Return the entire string -- let the pool figure it out.
  return sha256($str);
}

1;

__END__

# ABSTRACT: Random numbers using timer/schedule entropy

=pod

=head1 NAME

Crypt::Random::TESHA2 - Random numbers using timer/schedule entropy


=head1 VERSION

Version 0.01


=head1 WARNING

I<This module implements userspace voodoo entropy.  You should use a proper
O/S supplied entropy source such as /dev/random or the Win32 Crypt API.>


=head1 SYNOPSIS

  # Nothing exported by default
  use Crypt::Random::TESHA2 qw(random_bytes random_values irand rand);

  # Get 64 random bytes
  my $seed_string = random_bytes(64);

  # Get 16 random 32-bit values
  my @seeds = random_values(16);

  # Get a 32-bit random integer (value between 0 and 4294967295 inclusive)
  my $i = irand();

  # rand, like system rand, with 32 bits of randomness.
  my $r1 = rand();      # floating point in range [0,1).
  my $r2 = rand(1000);  # floating point in range [0,1000).


  # croak if installation determined we couldn't generate enough entropy
  use Crypt::Random::TESHA2 ':strong';

  # No warnings even if we are a weak source
  use Crypt::Random::TESHA2 ':weak';

  # Ask for yourself
  die "No key for you!" unless Crypt::Random::TESHA2::is_strong();


=head1 DESCRIPTION

Generate random numbers using entropy gathered from timer / scheduler jitter.

This can be used to generate non-pseudorandom data to seed a PRNG (e.g.
C<srand>/C<rand>, L<Math::Random::MT>, etc.) or CSPRNG (e.g. AES-CTR or
L<Math::Random::ISAAC>).  You may use it directly or as part of a random
source module that first checks for O/S randomness sources.

Only Perl CORE modules are used, making this very portable.  However, systems
must have a high resolution timer and support C<usleep> from L<Time::HiRes>.

At installation time, measurements are taken of the estimated entropy gathered
by the timer differences.  If the results indicated we could not get good
results, then the module will consider itself "weak".  On the first use of
any of the functions that return randomness (e.g. random_bytes), the module
will carp about not being a strong randomness source.  However, two special
options, ":strong" and ":weak" may be given to the importer to change this
behavior.  If ":strong" is used, then the module will croak.  If ":weak" is
used, then no carp will be generated.  The function C<is_strong> can be used
at any time for finer control.  Note that this should be an unusual case, and
neither flag has any effect if the module considers itself strong.


=head1 FUNCTIONS

=head2 random_bytes($n)

Takes an integer and returns a string of that size filled with random data.

=head2 random_values($n)

Takes an integer and returns an array containing that many random 32-bit
integers.  The values will be in the range [0,4294967295] (all 32-bit values
are possible).

=head2 irand

Returns a single random 32-bit integer in the range [0,4294967295].

=head2 rand

Returns a random float greater than or equal to 0 and less than the value of
the argument.  If no argument is given or the argument is zero, 1 is used.
This has an identical API as system rand, though of course there is no
associated srand function.  The result has 32 bits of randomness.

=head2 is_strong

Returns 0 if the installation procedure determined that not enough entropy
could be gathered on this system.  Returns 1 if it was able.  If 0 is
returned, then the bytes returned may be no better than a CSPRNG using a
convoluted time-based reseed every bit.


=head1 METHOD

The underlying entropy gathering is done using timing differences between
usleep calls.  We wrap usleep calls of varying intervals along with some
Perl hash processing inside microsecond timer calls.  The two values are
xored.  This is the raw entropy source.  Eight of these, along with the
current time, are fed to a SHA-256 which can be added to an entropy pool.

Measurements of the raw timer entropy (just the timing differences -- no
hashes, time, counters, xors, or entropy pool) on systems I have available
indicate 1.5 to 4 bits of entropy per usleep.  The installation procedure
does a measurement of the 0-order entropy gathered from the raw timing
process, halves it, limits to the range 1/8 - 7/8, and uses that as the
estimated entropy gathered.

The actual output random bytes are generated by an entropy pool that uses
SHA-512 or SHA-256.  This adds data as needed from the above method, then
extracts bits as needed to make the output bytes (again using a cryptographic
hash and a counter, which means the entropy pool is not exposed).

The result will easily pass most stream randomness tests (e.g. FIPS-140, ENT,
TestU01 Rabbit), but that is a given based on the last entropy pool stage, so
this just shows we provide decorrelated output, not that we make a good seed.


=head1 LIMITATIONS

Note that pretty much every limitation of this module will apply to
L<Math::TrulyRandom>, which many non-cryptographers still think is
cryptographically secure (it's recommended in both the perl core documentation
as well as L<Math::Random::ISAAC>).  If you think that module is great for
your application, then you should be happy with this one.  Probably happier
since this is more portable, doesn't hang infinitely, runs much faster,
and generates better output on most systems.

As mentioned in the L<Warnings|/"WARNINGS"> section, this generates userspace
entropy -- what most people used until the waning years of the 20th century.
We should not have to do this on modern systems that have well designed APIs
to get randomness from multiple entropy pools, all managed by production code.
In other words, C</dev/random>.

Performance is slow (about 10,000 times slower than L<Math::Random::ISAAC::XS>),
making this something best to be used to seed a PRNG or CSPRNG, rather than
using directly.  On newer Linux systems and Win32 it runs about 10,000 bits
per second.  Cygwin runs about 1000 bits per second.  Older systems will run
slower of course, such as an old HPPA system I use that runs at 40 bits/s.
Much of the time is spent sleeping.

Gathering entropy with this method depends on high resolution timers.  If the
timers have low resolution, especially if we had a system with very fast
yield turnaround, then we would gather very little entropy.  One of the tests
tries to determine this, but it isn't perfect.  As with all such userspace
systems, you should check that it works before using it for anything critical.
L<RFC4086|http://www.ietf.org/rfc/rfc4086.txt> section 3.4 discusses a few of
the pitfalls of using portable clock-based software, and section 3.6 discusses
the desire for multiple entropy sources.

Because of the use of SHA-2 hashes along with an entropy pool using a counter,
the output stream will pass randomness tests (e.g. FIPS-140, ENT,
TestU01 Rabbit) even if there is I<no> underlying entropy.  The installation
measurements should indicate whether this is happening, but it doesn't measure
everything.


=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>


=head1 SEE ALSO

=over 4

=item Encyclopedia of Cryptography and Security, volume 2, "Entropy Sources".
      The entropy pool implemented in this module follows this design.

=item HAVEGE (L<http://www.issihosts.com/haveged/>)
      Uses multiple methods to gather entropy and feed it to the O/S, which
      can measure it and add it to a pool.  Highly recommended for embedded
      or network devices that don't have good external interactions, or when
      running programs that use a lot of entropy (e.g. anything that uses
      L<Crypt::Random>).

=item timer_entropyd (L<http://www.vanheusden.com/te/>)
      Uses a related method (jitter in timing data between usleeps) as this
      module, but inefficient and only suitable for bulk feeding of an
      entropy pool.  Even after von Neumann debiasing, the output has distinct
      patterns and at most 0.5 bits of entropy per output bit.  HAVEGE is a
      superior overall solution.  However, note a number of other links at
      the site for other sources as well as links to hardware RNGs.

=item L<Math::TrulyRandom>
      An old module that uses an obsolete version of Matt Blaze's TrueRand.
      TrueRand version 2.1 fixes a number of issues with the output quality
      and specifically recommends against using the old method.  In addition,
      the Perl module will not properly run on most current platforms.
      A pure Perl version is included in the examples directory of this
      module, but it is still TrueRand version 1 and, like the old module,
      will not run on Win32.

=item L<Crypt::Urandom>
      A simple module that gets a good source of O/S non-blocking randomness.

=item L<Crypt::Random::Source>
      A complicated module that has multiple plugins for randomness sources.

=back


=head1 COPYRIGHT

Copyright 2012-2013 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

The software is provided "AS IS", without warranty of any kind, express or
implied, including but not limited to the warranties of merchantability,
fitness for a particular purpose and noninfringement. In no event shall the
authors or copyright holders be liable for any claim, damages or other
liability, whether in an action of contract, tort or otherwise, arising from,
out of or in connection with the software or the use or other dealings in
the software.

=cut
