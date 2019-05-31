
package Carp::Always;

use 5.006;
use strict;
use warnings;

our $VERSION = '0.16';
$VERSION =~ tr/_//d;

BEGIN {
  require Carp;
  $Carp::CarpInternal{ +__PACKAGE__ }++;
}

use constant CHOMP_DOT => $Carp::VERSION < 1.25;

sub _warn { warn &_longmess }

sub _die { die ref $_[0] ? @_ : &_longmess }

sub _longmess {
  if (CHOMP_DOT && $_[-1] =~ /\.\n\z/) {
    my $arg = pop @_;
    $arg =~ s/\.\n\z/\n/;
    push @_, $arg;
  }
  my $mess = &Carp::longmess;
  $mess =~ s/( at .*?\n)\1/$1/s;    # Suppress duplicate tracebacks
  $mess;
}

my @HOOKS = qw(__DIE__ __WARN__);
my %OLD_SIG;

sub import {
  my $class = shift;
  return if $OLD_SIG{$class};
  @{ $OLD_SIG{$class} }{ @HOOKS, 'Verbose' } = (@SIG{@HOOKS}, $Carp::Verbose);

  @SIG{@HOOKS} = ($class->can('_die'), $class->can('_warn'));
  $Carp::Verbose = 'verbose';    # makes carp() cluck and croak() confess
}

sub unimport {
  my $class = shift;
  return unless $OLD_SIG{$class};
  no if "$]" <= 5.008008, 'warnings' => 'uninitialized';
  (@SIG{@HOOKS}, $Carp::Verbose) = @{ delete $OLD_SIG{$class} }{ @HOOKS, 'Verbose' };
}

1;

=encoding utf8

=head1 NAME

Carp::Always - Warns and dies noisily with stack backtraces

=head1 SYNOPSIS

  use Carp::Always;

Often used on the command line:

  perl -MCarp::Always script.pl

=head1 DESCRIPTION

This module is meant as a debugging aid. It can be 
used to make a script complain loudly with stack backtraces 
when warn()ing or die()ing.

Here are how stack backtraces produced by this module
looks:

  # it works for explicit die's and warn's
  $ perl -MCarp::Always -e 'sub f { die "arghh" }; sub g { f }; g'
  arghh at -e line 1
          main::f() called at -e line 1
          main::g() called at -e line 1

  # it works for interpreter-thrown failures
  $ perl -MCarp::Always -w -e 'sub f { $a = shift; @a = @$a };' \
                           -e 'sub g { f(undef) }; g'
  Use of uninitialized value in array dereference at -e line 1
          main::f('undef') called at -e line 2
          main::g() called at -e line 2

In the implementation, the L<Carp> module does
the heavy work, through C<longmess()>. The
actual implementation sets the signal hooks
L<$SIG{__WARN__}|perlvar/%SIG> and L<$SIG{__DIE__}|perlvar/%SIG> to
emit the stack backtraces.

Also, all uses of C<carp> and C<croak> are made verbose,
behaving like C<cluck> and C<confess>.

=head1 METHODS

L<Carp::Always> implements the following methods.

=head2 import

  Carp::Always->import()

Enables L<Carp::Always>. Also triggered by statements like

  use Carp::Always;
  use Carp::Always 0.14;

but not by

  use Carp::Always ();    # does not invoke import()

=head2 unimport

  Carp::Always->unimport();

Disables L<Carp::Always>. Also triggered with

  no Carp::Always;

=head1 ACKNOWLEDGMENTS

This module was born as a reaction to a release
of L<Acme::JavaTrace> by Sébastien Aperghis-Tramoni.
Sébastien also has a newer module called
L<Devel::SimpleTrace> with the same code and fewer flame
comments on docs. The pruning of the uselessly long
docs of this module was prodded by Michael Schwern.

Schwern and others told me "the module name stinked" -
it was called C<Carp::Indeed>. After thinking long
and getting nowhere, I went with nuffin's suggestion
and now it is called C<Carp::Always>. 

=head1 SEE ALSO

L<Carp>

L<Acme::JavaTrace> and L<Devel::SimpleTrace>

L<Carp::Always::Color>

L<Carp::Source::Always>

L<Devel::Confess>

L<Carp::Always::SyntaxHighlightSource> and L<Carp::Always::DieOnly>

=head1 BUGS

=over 4

=item *

This module does not play well with other modules which fusses
around with C<warn>, C<die>, C<$SIG{__WARN__}>, C<$SIG{__DIE__}>.

=item *

Test scripts are good. I should write more of these.

=back

Please report bugs via GitHub
L<https://github.com/aferreira/cpan-Carp-Always/issues>

Backlog in CPAN RT: L<https://rt.cpan.org/Public/Dist/Display.html?Name=Carp-Always>

=head1 AUTHOR

Adriano Ferreira, E<lt>ferreira@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2005-2013, 2018 by Adriano Ferreira

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
