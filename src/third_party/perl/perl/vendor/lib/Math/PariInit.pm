package Math::Pari;

my %shift = ( k => 10, K => 10, m => 20, M => 20, g => 30, G=> 30);

sub _human2decimal {
  local $_ = shift;
  $_ <<= $shift{$1} if s/([kmg])$//i;
  $_
}

sub Math::PariInit::import {
  my $seen;
  CORE::shift;
  my @args = map { 
    /^:?(primes|stack)=(\d+((\.\d*)?[eE][-+]?\d+)?)[kKmMgG]?$/ 
      ? do { ($1 eq 'primes' ? $initprimes : $initmem) = _human2decimal $2;
	     $seen++;
	     () }
	: $_
  } @_;
  if ($seen && defined &Math::Pari::pari2iv) {
    require Carp;
    Carp::croak(
	"Can't set primelimit and stack size after Math::Pari is loaded")
  }
  require Math::Pari;
  @_ = ('Math::Pari', @args);
  goto &Math::Pari::import;
}

1;

=head1 NAME

Math::PariInit - load C<Math::Pari> with specified $primelimit and $initmem.

=head1 SYNOPSIS

  use Math::PariInit qw(:DEFAULT :int primes=1.2e7 stack=1e7 prime)
  $bigprime = prime(500000);

=head1 DESCRIPTION

C<use Math::PariInit> takes the same arguments as C<use Math::Pari>
with the addition of C<:primes=I<limit>> and C<:stack=I<bytes>> which
specify up to which number the initial list of primes should be
precalculated, and how large should be the arena for PARI calculations.

The arguments C<primes> and C<stack> cannot be specified if
Math::Pari is already loaded.

=head1 AUTHOR

Ilya Zakharevich L<ilyaz@cpan.org>

=cut

