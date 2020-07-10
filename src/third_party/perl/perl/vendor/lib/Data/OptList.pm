use strict;
use warnings;
package Data::OptList;
# ABSTRACT: parse and validate simple name/value option pairs
$Data::OptList::VERSION = '0.110';
use List::Util ();
use Params::Util ();
use Sub::Install 0.921 ();

#pod =head1 SYNOPSIS
#pod
#pod   use Data::OptList;
#pod
#pod   my $options = Data::OptList::mkopt([
#pod     qw(key1 key2 key3 key4),
#pod     key5 => { ... },
#pod     key6 => [ ... ],
#pod     key7 => sub { ... },
#pod     key8 => { ... },
#pod     key8 => [ ... ],
#pod   ]);
#pod
#pod ...is the same thing, more or less, as:
#pod
#pod   my $options = [
#pod     [ key1 => undef,        ],
#pod     [ key2 => undef,        ],
#pod     [ key3 => undef,        ],
#pod     [ key4 => undef,        ],
#pod     [ key5 => { ... },      ],
#pod     [ key6 => [ ... ],      ],
#pod     [ key7 => sub { ... },  ],
#pod     [ key8 => { ... },      ],
#pod     [ key8 => [ ... ],      ],
#pod   ]);
#pod
#pod =head1 DESCRIPTION
#pod
#pod Hashes are great for storing named data, but if you want more than one entry
#pod for a name, you have to use a list of pairs.  Even then, this is really boring
#pod to write:
#pod
#pod   $values = [
#pod     foo => undef,
#pod     bar => undef,
#pod     baz => undef,
#pod     xyz => { ... },
#pod   ];
#pod
#pod Just look at all those undefs!  Don't worry, we can get rid of those:
#pod
#pod   $values = [
#pod     map { $_ => undef } qw(foo bar baz),
#pod     xyz => { ... },
#pod   ];
#pod
#pod Aaaauuugh!  We've saved a little typing, but now it requires thought to read,
#pod and thinking is even worse than typing... and it's got a bug!  It looked right,
#pod didn't it?  Well, the C<< xyz => { ... } >> gets consumed by the map, and we
#pod don't get the data we wanted.
#pod
#pod With Data::OptList, you can do this instead:
#pod
#pod   $values = Data::OptList::mkopt([
#pod     qw(foo bar baz),
#pod     xyz => { ... },
#pod   ]);
#pod
#pod This works by assuming that any defined scalar is a name and any reference
#pod following a name is its value.
#pod
#pod =func mkopt
#pod
#pod   my $opt_list = Data::OptList::mkopt($input, \%arg);
#pod
#pod Valid arguments are:
#pod
#pod   moniker        - a word used in errors to describe the opt list; encouraged
#pod   require_unique - if true, no name may appear more than once
#pod   must_be        - types to which opt list values are limited (described below)
#pod   name_test      - a coderef used to test whether a value can be a name
#pod                    (described below, but you probably don't want this)
#pod
#pod This produces an array of arrays; the inner arrays are name/value pairs.
#pod Values will be either "undef" or a reference.
#pod
#pod Positional parameters may be used for compatibility with the old C<mkopt>
#pod interface:
#pod
#pod   my $opt_list = Data::OptList::mkopt($input, $moniker, $req_uni, $must_be);
#pod
#pod Valid values for C<$input>:
#pod
#pod  undef    -> []
#pod  hashref  -> [ [ key1 => value1 ] ... ] # non-ref values become undef
#pod  arrayref -> every name followed by a non-name becomes a pair: [ name => ref ]
#pod              every name followed by undef becomes a pair: [ name => undef ]
#pod              otherwise, it becomes [ name => undef ] like so:
#pod              [ "a", "b", [ 1, 2 ] ] -> [ [ a => undef ], [ b => [ 1, 2 ] ] ]
#pod
#pod By default, a I<name> is any defined non-reference.  The C<name_test> parameter
#pod can be a code ref that tests whether the argument passed it is a name or not.
#pod This should be used rarely.  Interactions between C<require_unique> and
#pod C<name_test> are not yet particularly elegant, as C<require_unique> just tests
#pod string equality.  B<This may change.>
#pod
#pod The C<must_be> parameter is either a scalar or array of scalars; it defines
#pod what kind(s) of refs may be values.  If an invalid value is found, an exception
#pod is thrown.  If no value is passed for this argument, any reference is valid.
#pod If C<must_be> specifies that values must be CODE, HASH, ARRAY, or SCALAR, then
#pod Params::Util is used to check whether the given value can provide that
#pod interface.  Otherwise, it checks that the given value is an object of the kind.
#pod
#pod In other words:
#pod
#pod   [ qw(SCALAR HASH Object::Known) ]
#pod
#pod Means:
#pod
#pod   _SCALAR0($value) or _HASH($value) or _INSTANCE($value, 'Object::Known')
#pod
#pod =cut

my %test_for;
BEGIN {
  %test_for = (
    CODE   => \&Params::Util::_CODELIKE,  ## no critic
    HASH   => \&Params::Util::_HASHLIKE,  ## no critic
    ARRAY  => \&Params::Util::_ARRAYLIKE, ## no critic
    SCALAR => \&Params::Util::_SCALAR0,   ## no critic
  );
}

sub mkopt {
  my ($opt_list) = shift;

  my ($moniker, $require_unique, $must_be); # the old positional args
  my ($name_test, $is_a);

  if (@_) {
    if (@_ == 1 and Params::Util::_HASHLIKE($_[0])) {
      ($moniker, $require_unique, $must_be, $name_test)
        = @{$_[0]}{ qw(moniker require_unique must_be name_test) };
    } else {
      ($moniker, $require_unique, $must_be) = @_;
    }

    # Transform the $must_be specification into a closure $is_a
    # that will check if a value matches the spec

    if (defined $must_be) {
      $must_be = [ $must_be ] unless ref $must_be;
      my @checks = map {
          my $class = $_;
          $test_for{$_}
          || sub { $_[1] = $class; goto \&Params::Util::_INSTANCE }
      } @$must_be;

      $is_a = (@checks == 1)
            ? $checks[0]
            : sub {
                my $value = $_[0];
                List::Util::first { defined($_->($value)) } @checks
              };

      $moniker = 'unnamed' unless defined $moniker;
    }
  }

  return [] unless $opt_list;

  $name_test ||= sub { ! ref $_[0] };

  $opt_list = [
    map { $_ => (ref $opt_list->{$_} ? $opt_list->{$_} : ()) } keys %$opt_list
  ] if ref $opt_list eq 'HASH';

  my @return;
  my %seen;

  for (my $i = 0; $i < @$opt_list; $i++) { ## no critic
    my $name = $opt_list->[$i];

    if ($require_unique) {
      Carp::croak "multiple definitions provided for $name" if $seen{$name}++;
    }

    my $value;

    if ($i < $#$opt_list) {
      if (not defined $opt_list->[$i+1]) {
        $i++
      } elsif (! $name_test->($opt_list->[$i+1])) {
        $value = $opt_list->[++$i];
        if ($is_a && !$is_a->($value)) {
          my $ref = ref $value;
          Carp::croak "$ref-ref values are not valid in $moniker opt list";
        }
      }
    }

    push @return, [ $name => $value ];
  }

  return \@return;
}

#pod =func mkopt_hash
#pod
#pod   my $opt_hash = Data::OptList::mkopt_hash($input, $moniker, $must_be);
#pod
#pod Given valid C<L</mkopt>> input, this routine returns a reference to a hash.  It
#pod will throw an exception if any name has more than one value.
#pod
#pod =cut

sub mkopt_hash {
  my ($opt_list, $moniker, $must_be) = @_;
  return {} unless $opt_list;

  $opt_list = mkopt($opt_list, $moniker, 1, $must_be);
  my %hash = map { $_->[0] => $_->[1] } @$opt_list;
  return \%hash;
}

#pod =head1 EXPORTS
#pod
#pod Both C<mkopt> and C<mkopt_hash> may be exported on request.
#pod
#pod =cut

BEGIN {
  *import = Sub::Install::exporter {
    exports => [qw(mkopt mkopt_hash)],
  };
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Data::OptList - parse and validate simple name/value option pairs

=head1 VERSION

version 0.110

=head1 SYNOPSIS

  use Data::OptList;

  my $options = Data::OptList::mkopt([
    qw(key1 key2 key3 key4),
    key5 => { ... },
    key6 => [ ... ],
    key7 => sub { ... },
    key8 => { ... },
    key8 => [ ... ],
  ]);

...is the same thing, more or less, as:

  my $options = [
    [ key1 => undef,        ],
    [ key2 => undef,        ],
    [ key3 => undef,        ],
    [ key4 => undef,        ],
    [ key5 => { ... },      ],
    [ key6 => [ ... ],      ],
    [ key7 => sub { ... },  ],
    [ key8 => { ... },      ],
    [ key8 => [ ... ],      ],
  ]);

=head1 DESCRIPTION

Hashes are great for storing named data, but if you want more than one entry
for a name, you have to use a list of pairs.  Even then, this is really boring
to write:

  $values = [
    foo => undef,
    bar => undef,
    baz => undef,
    xyz => { ... },
  ];

Just look at all those undefs!  Don't worry, we can get rid of those:

  $values = [
    map { $_ => undef } qw(foo bar baz),
    xyz => { ... },
  ];

Aaaauuugh!  We've saved a little typing, but now it requires thought to read,
and thinking is even worse than typing... and it's got a bug!  It looked right,
didn't it?  Well, the C<< xyz => { ... } >> gets consumed by the map, and we
don't get the data we wanted.

With Data::OptList, you can do this instead:

  $values = Data::OptList::mkopt([
    qw(foo bar baz),
    xyz => { ... },
  ]);

This works by assuming that any defined scalar is a name and any reference
following a name is its value.

=head1 FUNCTIONS

=head2 mkopt

  my $opt_list = Data::OptList::mkopt($input, \%arg);

Valid arguments are:

  moniker        - a word used in errors to describe the opt list; encouraged
  require_unique - if true, no name may appear more than once
  must_be        - types to which opt list values are limited (described below)
  name_test      - a coderef used to test whether a value can be a name
                   (described below, but you probably don't want this)

This produces an array of arrays; the inner arrays are name/value pairs.
Values will be either "undef" or a reference.

Positional parameters may be used for compatibility with the old C<mkopt>
interface:

  my $opt_list = Data::OptList::mkopt($input, $moniker, $req_uni, $must_be);

Valid values for C<$input>:

 undef    -> []
 hashref  -> [ [ key1 => value1 ] ... ] # non-ref values become undef
 arrayref -> every name followed by a non-name becomes a pair: [ name => ref ]
             every name followed by undef becomes a pair: [ name => undef ]
             otherwise, it becomes [ name => undef ] like so:
             [ "a", "b", [ 1, 2 ] ] -> [ [ a => undef ], [ b => [ 1, 2 ] ] ]

By default, a I<name> is any defined non-reference.  The C<name_test> parameter
can be a code ref that tests whether the argument passed it is a name or not.
This should be used rarely.  Interactions between C<require_unique> and
C<name_test> are not yet particularly elegant, as C<require_unique> just tests
string equality.  B<This may change.>

The C<must_be> parameter is either a scalar or array of scalars; it defines
what kind(s) of refs may be values.  If an invalid value is found, an exception
is thrown.  If no value is passed for this argument, any reference is valid.
If C<must_be> specifies that values must be CODE, HASH, ARRAY, or SCALAR, then
Params::Util is used to check whether the given value can provide that
interface.  Otherwise, it checks that the given value is an object of the kind.

In other words:

  [ qw(SCALAR HASH Object::Known) ]

Means:

  _SCALAR0($value) or _HASH($value) or _INSTANCE($value, 'Object::Known')

=head2 mkopt_hash

  my $opt_hash = Data::OptList::mkopt_hash($input, $moniker, $must_be);

Given valid C<L</mkopt>> input, this routine returns a reference to a hash.  It
will throw an exception if any name has more than one value.

=head1 EXPORTS

Both C<mkopt> and C<mkopt_hash> may be exported on request.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 CONTRIBUTORS

=for stopwords Olivier Mengué Ricardo SIGNES

=over 4

=item *

Olivier Mengué <dolmen@cpan.org>

=item *

Ricardo SIGNES <rjbs@codesimply.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2006 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
