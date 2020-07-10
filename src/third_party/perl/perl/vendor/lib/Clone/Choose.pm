package Clone::Choose;

use strict;
use warnings;
use Carp ();

our $VERSION = "0.010";
$VERSION = eval $VERSION;

our @BACKENDS = (
    Clone       => [0.10, "clone"],
    Storable    => "dclone",
    "Clone::PP" => "clone",
);

my $use_m;

BEGIN
{
    unless ($use_m)
    {
        eval "use Module::Runtime (); 1;"
          and $use_m = Module::Runtime->can("use_module")
          unless $ENV{CLONE_CHOOSE_NO_MODULE_RUNTIME};
        $use_m ||= sub {
            my ($pkg, @imports) = @_;
            my $use_stmt = "use $pkg";
            @imports and $use_stmt = join(" ", $use_stmt, @imports);
            eval $use_stmt;
            $@ and die $@;
            1;
        };
    }
}

sub backend
{
    my $self     = shift;
    my @backends = @BACKENDS;

    if ($ENV{CLONE_CHOOSE_PREFERRED_BACKEND})
    {
        my $favourite = $ENV{CLONE_CHOOSE_PREFERRED_BACKEND};
        my %b         = @backends;
        Carp::croak "$favourite not found" unless $b{$favourite};
        @backends = ($favourite => $b{$favourite});
    }

    while (my ($pkg, $rout) = splice @backends, 0, 2)
    {
        eval { $use_m->($pkg, ref $rout ? ($rout->[0]) : ()); 1; } or next;

        my $fn = $pkg->can(ref $rout ? $rout->[1] : $rout);
        $fn or next;

        return $pkg;
    }
}

sub can
{
    my $self     = shift;
    my $name     = shift;
    my @backends = @BACKENDS;

    return __PACKAGE__->SUPER::can($name) unless $name eq "clone";

    if ($ENV{CLONE_CHOOSE_PREFERRED_BACKEND})
    {
        my $favourite = $ENV{CLONE_CHOOSE_PREFERRED_BACKEND};
        my %b         = @backends;
        Carp::croak "$favourite not found" unless $b{$favourite};
        @backends = ($favourite => $b{$favourite});
    }

    my $fn;
    while (my ($pkg, $rout) = splice @backends, 0, 2)
    {
        eval { $use_m->($pkg, ref $rout ? ($rout->[0]) : ()); 1; } or next;

        $fn = $pkg->can(ref $rout ? $rout->[1] : $rout);
        $fn or next;

        last;
    }

    return $fn;
}

sub import
{
    my ($me, @params) = @_;
    my $tgt = caller(0);

    my @B = @BACKENDS;
    local @BACKENDS = @B;

    push @params, "clone" unless grep { /^clone$/ } @params;

    while (my $param = shift @params)
    {
        if ($param =~ m/^:(.*)$/)
        {
            my $favourite = $1;
            $ENV{CLONE_CHOOSE_PREFERRED_BACKEND}
              and $ENV{CLONE_CHOOSE_PREFERRED_BACKEND} ne $favourite
              and Carp::croak
              "Environment CLONE_CHOOSE_PREFERRED_BACKEND($ENV{CLONE_CHOOSE_PREFERRED_BACKEND}) not equal to imported ($favourite)";

            my %b = @BACKENDS;
            Carp::croak "$favourite not found" unless $b{$favourite};
            @BACKENDS = ($favourite => $b{$favourite});
        }
        elsif ($param eq "clone")
        {
            my $fn = __PACKAGE__->can("clone");
            $fn or Carp::croak "Cannot find an apropriate clone().";

            no strict "refs";
            *{"${tgt}::clone"} = $fn;

            @params
              and Carp::croak "Parameters left after clone. Please see description.";

            return;
        }
        else
        {
            Carp::croak "$param is not exportable by " . __PACKAGE__;
        }
    }
}

sub get_backends
{
    my $self     = shift;
    my %backends = @BACKENDS;

    if ($ENV{CLONE_CHOOSE_PREFERRED_BACKEND})
    {
        my $favourite = $ENV{CLONE_CHOOSE_PREFERRED_BACKEND};
        Carp::croak "$favourite not found" unless $backends{$favourite};
        %backends = ($favourite => $backends{$favourite});
    }

    return keys %backends;
}

1;

__END__

=head1 NAME

Clone::Choose - Choose appropriate clone utility

=begin html

<a href="https://travis-ci.org/perl5-utils/Clone-Choose"><img src="https://travis-ci.org/perl5-utils/Clone-Choose.svg?branch=master" alt="Travis CI"/></a>
<a href='https://coveralls.io/github/perl5-utils/Clone-Choose?branch=master'><img src='https://coveralls.io/repos/github/perl5-utils/Clone-Choose/badge.svg?branch=master' alt='Coverage Status'/></a>

=end html

=head1 SYNOPSIS

  use Clone::Choose;

  my $data = {
      value => 42,
      href  => {
          set   => [ 'foo', 'bar' ],
          value => 'baz',
      },
  };

  my $cloned_data = clone $data;

  # it's also possible to use Clone::Choose and pass a clone preference
  use Clone::Choose qw(:Storable);

=head1 DESCRIPTION

C<Clone::Choose> checks several different modules which provides a
C<clone()> function and selects an appropriate one. The default preference
is

  Clone
  Storable
  Clone::PP

This list might evolve in future. Please see L</EXPORTS> how to pick a
particular one.

=head1 EXPORTS

C<Clone::Choose> exports C<clone()> by default.

One can explicitly import C<clone> by using

  use Clone::Choose qw(clone);

or pick a particular C<clone> implementation

  use Clone::Choose qw(:Storable clone);

The exported implementation is resolved dynamically, which means that any
using module can either rely on the default backend preference or choose
a particular one.

It is also possible to select a particular C<clone> backend by setting the
environment variable CLONE_CHOOSE_PREFERRED_BACKEND to your preferred backend.

This also means, an already chosen import can't be modified like

  use Clone::Choose qw(clone :Storable);

When one seriously needs different clone implementations, our I<recommended>
way to use them would be:

  use Clone::Choose (); # do not import
  my ($xs_clone, $st_clone);
  { local @Clone::Choose::BACKENDS = (Clone => "clone"); $xs_clone = Clone::Choose->can("clone"); }
  { local @Clone::Choose::BACKENDS = (Storable => "dclone"); $st_clone = Clone::Choose->can("clone"); }

Don't misinterpret I<recommended> - modifying C<@Clone::Choose::BACKENDS>
has a lot of pitfalls and is unreliable beside such small examples. Do
not hesitate open a request with an appropriate proposal for choosing
implementations dynamically.

The use of C<@Clone::Choose::BACKENDS> is discouraged and will be deprecated
as soon as anyone provides a better idea.

=head1 PACKAGE METHODS

=head2 backend

C<backend> tells the caller about the dynamic chosen backend:

  use Clone::Choose;
  say Clone::Choose->backend; # Clone

This method currently exists for debug purposes only.

=head2 get_backends

C<get_backends> returns a list of the currently supported backends.

=head1 AUTHOR

  Jens Rehsack <rehsack at cpan dot org>
  Stefan Hermes <hermes at cpan dot org>

=head1 BUGS

Please report any bugs or feature requests to
C<bug-Clone-Choose at rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Clone-Choose>.
I will be notified, and then you'll automatically be notified of progress
on your bug as I make changes.

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

  perldoc Clone::Choose

You can also look for information at:

=over 4

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=Clone-Choose>

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/Clone-Choose>

=item * CPAN Ratings

L<http://cpanratings.perl.org/d/Clone-Choose>

=item * Search CPAN

L<http://search.cpan.org/dist/Clone-Choose/>

=back

=head1 LICENSE AND COPYRIGHT

  Copyright 2017 Jens Rehsack
  Copyright 2017 Stefan Hermes

This program is free software; you can redistribute it and/or modify it
under the terms of either: the GNU General Public License as published
by the Free Software Foundation; or the Artistic License.

See http://dev.perl.org/licenses/ for more information.

=head1 SEE ALSO

L<Clone>, L<Clone::PP>, L<Storable>

=cut
