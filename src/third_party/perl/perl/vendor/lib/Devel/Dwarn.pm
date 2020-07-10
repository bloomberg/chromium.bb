package Devel::Dwarn;

use Data::Dumper::Concise::Sugar;

sub import {
  Data::Dumper::Concise::Sugar->export_to_level(1, @_);
}

=head1 NAME

Devel::Dwarn - Combine warns and Data::Dumper::Concise

=head1 SYNOPSIS

  use Devel::Dwarn;

  return Dwarn some_call(...)

is equivalent to:

  use Data::Dumper::Concise;

  if (wantarray) {
     my @return = some_call(...);
     warn Dumper(@return);
     return @return;
  } else {
     my $return = some_call(...);
     warn Dumper($return);
     return $return;
  }

but shorter. If you need to force scalar context on the value,

  use Devel::Dwarn;

  return DwarnS some_call(...)

is equivalent to:

  use Data::Dumper::Concise;

  my $return = some_call(...);
  warn Dumper($return);
  return $return;

If you need to force list context on the value,

  use Devel::Dwarn;

  return DwarnL some_call(...)

is equivalent to:

  use Data::Dumper::Concise;

  my @return = some_call(...);
  warn Dumper(@return);
  return @return;

If you want to label your output, try DwarnN

  use Devel::Dwarn;

  return DwarnN $foo

is equivalent to:

  use Data::Dumper::Concise;

  my @return = some_call(...);
  warn '$foo => ' . Dumper(@return);
  return @return;

If you want to output a reference returned by a method easily, try $Dwarn

 $foo->bar->{baz}->$Dwarn

is equivalent to:

  my $return = $foo->bar->{baz};
  warn Dumper($return);
  return $return;

If you want to immediately die after outputting the data structure, every
Dwarn subroutine has a paired Ddie version, so just replace the warn with die.
For example:

 DdieL 'foo', { bar => 'baz' };

=head1 TIPS AND TRICKS

=head2 global usage

Instead of always just doing:

  use Devel::Dwarn;

  Dwarn ...

We tend to do:

  perl -MDevel::Dwarn foo.pl

(and then in the perl code:)

  ::Dwarn ...

That way, if you leave them in and run without the C<< use Devel::Dwarn >>
the program will fail to compile and you are less likely to check it in by
accident.  Furthmore it allows that much less friction to add debug messages.

=head2 method chaining

One trick which is useful when doing method chaining is the following:

  my $foo = Bar->new;
  $foo->bar->baz->Devel::Dwarn::DwarnS->biff;

which is the same as:

  my $foo = Bar->new;
  (DwarnS $foo->bar->baz)->biff;

=head1 SEE ALSO

This module is really just a shortcut for L<Data::Dumper::Concise::Sugar>, check
it out for more complete documentation.

=cut

1;
