package Data::Dumper::Concise::Sugar;

use 5.006;

our $VERSION = '2.023';

use Exporter ();
use Data::Dumper::Concise ();

BEGIN { @ISA = qw(Exporter) }

@EXPORT = qw(
   $Dwarn $DwarnN Dwarn DwarnS DwarnL DwarnN DwarnF
   $Ddie $DdieN Ddie DdieS DdieL DdieN DdieD
);

sub Dwarn { DwarnL(@_); return wantarray ? @_ : $_[0] }

our $Dwarn = \&Dwarn;
our $DwarnN = \&DwarnN;

sub DwarnL { warn Data::Dumper::Concise::Dumper @_; @_ }

sub DwarnS ($) { warn Data::Dumper::Concise::Dumper $_[0]; $_[0] }

sub DwarnN ($) {
   require Devel::ArgNames;
   my $x = Devel::ArgNames::arg_names();
   warn(($x?$x:'(anon)') . ' => ' . Data::Dumper::Concise::Dumper $_[0]); $_[0]
}

sub DwarnF (&@) { my $c = shift; warn &Data::Dumper::Concise::DumperF($c, @_); @_ }

sub Ddie { DdieL(@_); return wantarray ? @_ : $_[0] }

our $Ddie = \&Ddie;
our $DdieN = \&DdieN;

sub DdieL { die Data::Dumper::Concise::Dumper @_ }

sub DdieS ($) { die Data::Dumper::Concise::Dumper $_[0] }

sub DdieN ($) {
   require Devel::ArgNames;
   my $x = Devel::ArgNames::arg_names();
   die(($x?$x:'(anon)') . ' => ' . Data::Dumper::Concise::Dumper $_[0]);
}

=head1 NAME

Data::Dumper::Concise::Sugar - return Dwarn @return_value

=head1 SYNOPSIS

  use Data::Dumper::Concise::Sugar;

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

  use Data::Dumper::Concise::Sugar;

  return DwarnS some_call(...)

is equivalent to:

  use Data::Dumper::Concise;

  my $return = some_call(...);
  warn Dumper($return);
  return $return;

If you need to force list context on the value,

  use Data::Dumper::Concise::Sugar;

  return DwarnL some_call(...)

is equivalent to:

  use Data::Dumper::Concise;

  my @return = some_call(...);
  warn Dumper(@return);
  return @return;

If you want to label your output, try DwarnN

  use Data::Dumper::Concise::Sugar;

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

If you want to format the output of your data structures, try DwarnF

 my ($a, $c) = DwarnF { "awesome: $_[0] not awesome: $_[1]" } $awesome, $cheesy;

is equivalent to:

  my @return = ($awesome, $cheesy);
  warn DumperF { "awesome: $_[0] not awesome: $_[1]" } $awesome, $cheesy;
  return @return;

If you want to immediately die after outputting the data structure, every
Dwarn subroutine has a paired Ddie version, so just replace the warn with die.
For example:

 DdieL 'foo', { bar => 'baz' };

=head1 DESCRIPTION

  use Data::Dumper::Concise::Sugar;

will import Dwarn, $Dwarn, DwarnL, DwarnN, and DwarnS into your namespace. Using
L<Exporter>, so see its docs for ways to make it do something else.

=head2 Dwarn

  sub Dwarn { return DwarnL(@_) if wantarray; DwarnS($_[0]) }

=head2 $Dwarn

  $Dwarn = \&Dwarn

=head2 $DwarnN

  $DwarnN = \&DwarnN

=head2 DwarnL

  sub Dwarn { warn Data::Dumper::Concise::Dumper @_; @_ }

=head2 DwarnS

  sub DwarnS ($) { warn Data::Dumper::Concise::Dumper $_[0]; $_[0] }

=head2 DwarnN

  sub DwarnN { warn '$argname => ' . Data::Dumper::Concise::Dumper $_[0]; $_[0] }

B<Note>: this requires L<Devel::ArgNames> to be installed.

=head2 DwarnF

  sub DwarnF (&@) { my $c = shift; warn &Data::Dumper::Concise::DumperF($c, @_); @_ }

=head1 TIPS AND TRICKS

=head2 global usage

Instead of always just doing:

  use Data::Dumper::Concise::Sugar;

  Dwarn ...

We tend to do:

  perl -MData::Dumper::Concise::Sugar foo.pl

(and then in the perl code:)

  ::Dwarn ...

That way, if you leave them in and run without the
C<< use Data::Dumper::Concise::Sugar >> the program will fail to compile and
you are less likely to check it in by accident.  Furthmore it allows that
much less friction to add debug messages.

=head2 method chaining

One trick which is useful when doing method chaining is the following:

  my $foo = Bar->new;
  $foo->bar->baz->Data::Dumper::Concise::Sugar::DwarnS->biff;

which is the same as:

  my $foo = Bar->new;
  (DwarnS $foo->bar->baz)->biff;

=head1 SEE ALSO

You probably want L<Devel::Dwarn>, it's the shorter name for this module.

=cut

1;
