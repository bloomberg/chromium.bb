package Math::Prime::Util::PrimeIterator;
use strict;
use warnings;

BEGIN {
  $Math::Prime::Util::PrimeIterator::AUTHORITY = 'cpan:DANAJ';
  $Math::Prime::Util::PrimeIterator::VERSION = '0.73';
}

use base qw( Exporter );
our @EXPORT_OK = qw( );
our %EXPORT_TAGS = (all => [ @EXPORT_OK ]);


use Math::Prime::Util qw/next_prime prev_prime is_prime prime_count nth_prime/;

# We're going to use a scalar rather than a hash because there is currently
# only one data object (the current value) and this makes it little faster.

sub new {
  my ($class, $start) = @_;
  my $p = 2;
  my $self = bless \$p, $class;
  $self->rewind($start) if defined $start;
  return $self;
}

# To make Iterator::Simple happy.
sub __iter__ {
  my $self = shift;
  require Iterator::Simple;
  return Iterator::Simple::iterator(sub { $self->iterate });
  $self;
}

sub value { ${$_[0]}; }
sub next {
  #my $self = shift;  $$self = next_prime($$self);  return $self;
  ${$_[0]} = next_prime(${$_[0]});
  return $_[0];
}
sub prev {
  my $self = shift;
  my $p = $$self;
  $$self = ($p <= 2) ? 2 : prev_prime($p);
  return $self;
}
sub iterate {
  #my $self = shift;  my $p = $$self;  $$self = next_prime($p);  return $p;
  my $p = ${$_[0]};
  ${$_[0]} = next_prime(${$_[0]});
  return $p;
}

sub rewind {
  my ($self, $start) = @_;
  $$self = 2;
  if (defined $start && $start ne '2') {
    Math::Prime::Util::_validate_num($start)
      || Math::Prime::Util::_validate_positive_integer($start);
    $$self = next_prime($start-1) if $start > 2;
  }
  return $self;
}

sub peek {
  return next_prime(${$_[0]});
}

# Some methods to match Math::NumSeq
sub tell_i {
  return prime_count(${$_[0]});
}
sub pred {
  my($self, $n) = @_;
  return is_prime($n);
}
sub ith {
  my($self, $n) = @_;
  return nth_prime($n);
}
sub seek_to_i {
  my($self, $n) = @_;
  $self->rewind( nth_prime($n) );
}
sub seek_to_value {
  my($self, $n) = @_;
  $self->rewind($n);
}
sub value_to_i {
  my($self, $n) = @_;
  return unless is_prime($n);
  return prime_count($n);
}
sub value_to_i_ceil {
  my($self, $n) = @_;
  return prime_count(next_prime($n-1));
}
sub value_to_i_floor {
  my($self, $n) = @_;
  return prime_count($n);
}
sub value_to_i_estimate {
  my($self, $n) = @_;
  return Math::Prime::Util::prime_count_approx($n);
}
sub i_start     { 1 }
sub description { "The prime numbers 2, 3, 5, 7, 11, 13, 17, etc." }
sub values_min  { 2 }
sub values_max  { undef }
sub oeis_anum   { "A000040" }
1;

__END__


# ABSTRACT: An object iterator for primes

=pod

=for stopwords prev pred ith i'th

=for test_synopsis use v5.14;  my ($i,$n) = (2,2);

=head1 NAME

Math::Prime::Util::PrimeIterator - An object iterator for primes


=head1 VERSION

Version 0.73


=head1 SYNOPSIS

  use Math::Prime::Util::PrimeIterator;
  my $it = Math::Prime::Util::PrimeIterator->new();

  # Simple use: return current value and move forward.
  my $sum = 0;  $sum += $it->iterate() for 1..10000;

  # Methods
  my $v = $it->value();     # Return current value
  $it->next();              # Move to next prime (returns self)
  $it->prev();              # Move to prev prime (returns self)
  $v = $it->iterate();      # Returns current value; moves to next prime
  $it->rewind();            # Resets position to 2
  $it->rewind($n);          # Resets position to next_prime($n-1)

  # Methods similar to Math::NumSeq, do not change iterator
  $it->tell_i();            # Returns the index of the current position
  $it->pred($n);            # Returns true if $n is prime
  $it->ith($i);             # Returns the $ith prime
  $it->value_to_i($n);      # Returns the index of the first prime >= $n
  $it->value_to_i_estimate($n);  # Approx index of value $n

  # Methods similar to Math::NumSeq, changes iterator
  $it->seek_to_i($i);       # Resets position to the $ith prime
  $it->seek_to_value($i);   # Resets position to next_prime($i-1)

=head1 DESCRIPTION

An iterator over the primes.  L</new> returns an iterator object and takes
an optional starting position (the initial value will be the least prime
greater than or equal to the argument).  BigInt objects will be returned if
the value overflows a Perl unsigned integer value.

=head1 METHODS

=head2 new

Creates an iterator object with initial value of 2.  If an argument is
given, the initial value will be the least prime greater than or equal
to the argument.

=head2 value

Returns the value at the current position.  Will always be a prime.  If
the value is greater than ~0, it will be a L<Math::BigInt> object.

=head2 next

Moves the current position to the next prime.
Returns self so calls can be chained.

=head2 prev

Moves the current position to the previous prime, unless the current
value is 2, in which case the value remains 2.
Returns self so calls can be chained.

=head2 iterate

Returns the value at the current position and also moves the position to
the next prime.

=head2 rewind

Resets the current position to either 2 or, if given an integer argument,
the least prime not less than the argument.

=head2 peek

Returns the value at the next position without moving the iterator.

=head2 tell_i

Returns the index of the current position, starting at 1 (corresponding to
the value 2).
The iterator is unchanged after this call.

=head2 pred

Returns true if the argument is a prime, false otherwise.
The iterator is unchanged after this call.

=head2 ith

Returns the i'th prime, where the first prime is 2.
The iterator is unchanged after this call.

=head2 value_to_i_estimate

Returns an estimate of the index corresponding to the argument.  That is,
given a value C<n>, we expect a prime approximately equal to C<n> to occur
at this index.

The estimate is performed using L<Math::Prime::Util/prime_count_approx>,
which uses the estimates of Dusart 2010 (or better for small values).

=head2 value_to_i

If the argument is prime, returns the corresponding index, such that:

  ith( value_to_i( $n ) ) == $n

Returns C<undef> if the argument is not prime.

=head2 value_to_i_floor

=head2 value_to_i_ceil

Returns the index corresponding to the first prime less than or equal
to the argument, or greater than or equal to the argument, respectively.

=head2 seek_to_i

Resets the position to the prime corresponding to the given index.

=head2 seek_to_value

An alias for L</rewind>.

=head2 i_start
=head2 description
=head2 values_min
=head2 values_max
=head2 oeis_anum

Methods to match Math::NumSeq::Primes.

=head1 SEE ALSO

L<Math::Prime::Util>

L<Math::Prime::Util/forprimes>

L<Math::Prime::Util/prime_iterator>

L<Math::Prime::Util/prime_iterator_object>

L<Math::Prime::Util::PrimeArray>

L<Math::NumSeq::Primes>

L<List::Gen>

=head1 AUTHORS

Dana Jacobsen E<lt>dana@acm.orgE<gt>


=head1 COPYRIGHT

Copyright 2013 by Dana Jacobsen E<lt>dana@acm.orgE<gt>

This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut
