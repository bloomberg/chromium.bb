package namespace::clean;

use warnings;
use strict;

use vars qw( $STORAGE_VAR );
use Package::Stash;

our $VERSION = '0.23';

$STORAGE_VAR = '__NAMESPACE_CLEAN_STORAGE';

# FIXME - all of this buggery will migrate to B::H::EOS soon
BEGIN {
  # when changing also change in Makefile.PL
  my $b_h_eos_req = '0.10';

  if (! $ENV{NAMESPACE_CLEAN_USE_PP} and eval {
    require B::Hooks::EndOfScope;
    B::Hooks::EndOfScope->VERSION($b_h_eos_req);
    1
  } ) {
    B::Hooks::EndOfScope->import('on_scope_end');
  }
  elsif ($] < 5.009_003_1) {
    require namespace::clean::_PP_OSE_5_8;
    *on_scope_end = \&namespace::clean::_PP_OSE_5_8::on_scope_end;
  }
  else {
    require namespace::clean::_PP_OSE;
    *on_scope_end = \&namespace::clean::_PP_OSE::on_scope_end;
  }
}

=head1 NAME

namespace::clean - Keep imports and functions out of your namespace

=head1 SYNOPSIS

  package Foo;
  use warnings;
  use strict;

  use Carp qw(croak);   # 'croak' will be removed

  sub bar { 23 }        # 'bar' will be removed

  # remove all previously defined functions
  use namespace::clean;

  sub baz { bar() }     # 'baz' still defined, 'bar' still bound

  # begin to collection function names from here again
  no namespace::clean;

  sub quux { baz() }    # 'quux' will be removed

  # remove all functions defined after the 'no' unimport
  use namespace::clean;

  # Will print: 'No', 'No', 'Yes' and 'No'
  print +(__PACKAGE__->can('croak') ? 'Yes' : 'No'), "\n";
  print +(__PACKAGE__->can('bar')   ? 'Yes' : 'No'), "\n";
  print +(__PACKAGE__->can('baz')   ? 'Yes' : 'No'), "\n";
  print +(__PACKAGE__->can('quux')  ? 'Yes' : 'No'), "\n";

  1;

=head1 DESCRIPTION

=head2 Keeping packages clean

When you define a function, or import one, into a Perl package, it will
naturally also be available as a method. This does not per se cause
problems, but it can complicate subclassing and, for example, plugin
classes that are included via multiple inheritance by loading them as
base classes.

The C<namespace::clean> pragma will remove all previously declared or
imported symbols at the end of the current package's compile cycle.
Functions called in the package itself will still be bound by their
name, but they won't show up as methods on your class or instances.

By unimporting via C<no> you can tell C<namespace::clean> to start
collecting functions for the next C<use namespace::clean;> specification.

You can use the C<-except> flag to tell C<namespace::clean> that you
don't want it to remove a certain function or method. A common use would
be a module exporting an C<import> method along with some functions:

  use ModuleExportingImport;
  use namespace::clean -except => [qw( import )];

If you just want to C<-except> a single sub, you can pass it directly.
For more than one value you have to use an array reference.

=head2 Explicitly removing functions when your scope is compiled

It is also possible to explicitly tell C<namespace::clean> what packages
to remove when the surrounding scope has finished compiling. Here is an
example:

  package Foo;
  use strict;

  # blessed NOT available

  sub my_class {
      use Scalar::Util qw( blessed );
      use namespace::clean qw( blessed );

      # blessed available
      return blessed shift;
  }

  # blessed NOT available

=head2 Moose

When using C<namespace::clean> together with L<Moose> you want to keep
the installed C<meta> method. So your classes should look like:

  package Foo;
  use Moose;
  use namespace::clean -except => 'meta';
  ...

Same goes for L<Moose::Role>.

=head2 Cleaning other packages

You can tell C<namespace::clean> that you want to clean up another package
instead of the one importing. To do this you have to pass in the C<-cleanee>
option like this:

  package My::MooseX::namespace::clean;
  use strict;

  use namespace::clean (); # no cleanup, just load

  sub import {
      namespace::clean->import(
        -cleanee => scalar(caller),
        -except  => 'meta',
      );
  }

If you don't care about C<namespace::clean>s discover-and-C<-except> logic, and
just want to remove subroutines, try L</clean_subroutines>.

=head1 METHODS

=head2 clean_subroutines

This exposes the actual subroutine-removal logic.

  namespace::clean->clean_subroutines($cleanee, qw( subA subB ));

will remove C<subA> and C<subB> from C<$cleanee>. Note that this will remove the
subroutines B<immediately> and not wait for scope end. If you want to have this
effect at a specific time (e.g. C<namespace::clean> acts on scope compile end)
it is your responsibility to make sure it runs at that time.

=cut

# Constant to optimise away the unused code branches
use constant FIXUP_NEEDED => $] < 5.015_005_1;
use constant FIXUP_RENAME_SUB => $] > 5.008_008_9 && $] < 5.013_005_1;
{
  no strict;
  delete ${__PACKAGE__."::"}{FIXUP_NEEDED};
  delete ${__PACKAGE__."::"}{FIXUP_RENAME_SUB};
}

# Debugger fixup necessary before perl 5.15.5
#
# In perl 5.8.9-5.12, it assumes that sub_fullname($sub) can
# always be used to find the CV again.
# In perl 5.8.8 and 5.14, it assumes that the name of the glob
# passed to entersub can be used to find the CV.
# since we are deleting the glob where the subroutine was originally
# defined, those assumptions no longer hold.
#
# So in 5.8.9-5.12 we need to move it elsewhere and point the
# CV's name to the new glob.
#
# In 5.8.8 and 5.14 we move it elsewhere and rename the
# original glob by assigning the new glob back to it.
my $sub_utils_loaded;
my $DebuggerFixup = sub {
  my ($f, $sub, $cleanee_stash, $deleted_stash) = @_;

  if (FIXUP_RENAME_SUB) {
    if (! defined $sub_utils_loaded ) {
      $sub_utils_loaded = do {

        # when changing version also change in Makefile.PL
        my $sn_ver = 0.04;
        eval { require Sub::Name; Sub::Name->VERSION($sn_ver) }
          or die "Sub::Name $sn_ver required when running under -d or equivalent: $@";

        # when changing version also change in Makefile.PL
        my $si_ver = 0.04;
        eval { require Sub::Identify; Sub::Identify->VERSION($si_ver) }
          or die "Sub::Identify $si_ver required when running under -d or equivalent: $@";

        1;
      } ? 1 : 0;
    }

    if ( Sub::Identify::sub_fullname($sub) eq ($cleanee_stash->name . "::$f") ) {
      my $new_fq = $deleted_stash->name . "::$f";
      Sub::Name::subname($new_fq, $sub);
      $deleted_stash->add_symbol("&$f", $sub);
    }
  }
  else {
    $deleted_stash->add_symbol("&$f", $sub);
  }
};

my $RemoveSubs = sub {
    my $cleanee = shift;
    my $store   = shift;
    my $cleanee_stash = Package::Stash->new($cleanee);
    my $deleted_stash;

  SYMBOL:
    for my $f (@_) {

        # ignore already removed symbols
        next SYMBOL if $store->{exclude}{ $f };

        my $sub = $cleanee_stash->get_symbol("&$f")
          or next SYMBOL;

        my $need_debugger_fixup =
          FIXUP_NEEDED
            &&
          $^P
            &&
          ref(my $globref = \$cleanee_stash->namespace->{$f}) eq 'GLOB'
        ;

        if (FIXUP_NEEDED && $need_debugger_fixup) {
          # convince the Perl debugger to work
          # see the comment on top of $DebuggerFixup
          $DebuggerFixup->(
            $f,
            $sub,
            $cleanee_stash,
            $deleted_stash ||= Package::Stash->new("namespace::clean::deleted::$cleanee"),
          );
        }

        my @symbols = map {
            my $name = $_ . $f;
            my $def = $cleanee_stash->get_symbol($name);
            defined($def) ? [$name, $def] : ()
        } '$', '@', '%', '';

        $cleanee_stash->remove_glob($f);

        # if this perl needs no renaming trick we need to
        # rename the original glob after the fact
        # (see commend of $DebuggerFixup
        if (FIXUP_NEEDED && !FIXUP_RENAME_SUB && $need_debugger_fixup) {
          *$globref = $deleted_stash->namespace->{$f};
        }

        $cleanee_stash->add_symbol(@$_) for @symbols;
    }
};

sub clean_subroutines {
    my ($nc, $cleanee, @subs) = @_;
    $RemoveSubs->($cleanee, {}, @subs);
}

=head2 import

Makes a snapshot of the current defined functions and installs a
L<B::Hooks::EndOfScope> hook in the current scope to invoke the cleanups.

=cut

sub import {
    my ($pragma, @args) = @_;

    my (%args, $is_explicit);

  ARG:
    while (@args) {

        if ($args[0] =~ /^\-/) {
            my $key = shift @args;
            my $value = shift @args;
            $args{ $key } = $value;
        }
        else {
            $is_explicit++;
            last ARG;
        }
    }

    my $cleanee = exists $args{ -cleanee } ? $args{ -cleanee } : scalar caller;
    if ($is_explicit) {
        on_scope_end {
            $RemoveSubs->($cleanee, {}, @args);
        };
    }
    else {

        # calling class, all current functions and our storage
        my $functions = $pragma->get_functions($cleanee);
        my $store     = $pragma->get_class_store($cleanee);
        my $stash     = Package::Stash->new($cleanee);

        # except parameter can be array ref or single value
        my %except = map {( $_ => 1 )} (
            $args{ -except }
            ? ( ref $args{ -except } eq 'ARRAY' ? @{ $args{ -except } } : $args{ -except } )
            : ()
        );

        # register symbols for removal, if they have a CODE entry
        for my $f (keys %$functions) {
            next if     $except{ $f };
            next unless $stash->has_symbol("&$f");
            $store->{remove}{ $f } = 1;
        }

        # register EOF handler on first call to import
        unless ($store->{handler_is_installed}) {
            on_scope_end {
                $RemoveSubs->($cleanee, $store, keys %{ $store->{remove} });
            };
            $store->{handler_is_installed} = 1;
        }

        return 1;
    }
}

=head2 unimport

This method will be called when you do a

  no namespace::clean;

It will start a new section of code that defines functions to clean up.

=cut

sub unimport {
    my ($pragma, %args) = @_;

    # the calling class, the current functions and our storage
    my $cleanee   = exists $args{ -cleanee } ? $args{ -cleanee } : scalar caller;
    my $functions = $pragma->get_functions($cleanee);
    my $store     = $pragma->get_class_store($cleanee);

    # register all unknown previous functions as excluded
    for my $f (keys %$functions) {
        next if $store->{remove}{ $f }
             or $store->{exclude}{ $f };
        $store->{exclude}{ $f } = 1;
    }

    return 1;
}

=head2 get_class_store

This returns a reference to a hash in a passed package containing
information about function names included and excluded from removal.

=cut

sub get_class_store {
    my ($pragma, $class) = @_;
    my $stash = Package::Stash->new($class);
    my $var = "%$STORAGE_VAR";
    $stash->add_symbol($var, {})
        unless $stash->has_symbol($var);
    return $stash->get_symbol($var);
}

=head2 get_functions

Takes a class as argument and returns all currently defined functions
in it as a hash reference with the function name as key and a typeglob
reference to the symbol as value.

=cut

sub get_functions {
    my ($pragma, $class) = @_;

    my $stash = Package::Stash->new($class);
    return {
        map { $_ => $stash->get_symbol("&$_") }
            $stash->list_all_symbols('CODE')
    };
}

=head1 IMPLEMENTATION DETAILS

This module works through the effect that a

  delete $SomePackage::{foo};

will remove the C<foo> symbol from C<$SomePackage> for run time lookups
(e.g., method calls) but will leave the entry alive to be called by
already resolved names in the package itself. C<namespace::clean> will
restore and therefor in effect keep all glob slots that aren't C<CODE>.

A test file has been added to the perl core to ensure that this behaviour
will be stable in future releases.

Just for completeness sake, if you want to remove the symbol completely,
use C<undef> instead.

=head1 SEE ALSO

L<B::Hooks::EndOfScope>

=head1 THANKS

Many thanks to Matt S Trout for the inspiration on the whole idea.

=head1 AUTHORS

=over

=item *

Robert 'phaylon' Sedlacek <rs@474.at>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

Peter Rabbitson <ribasushi@cpan.org>

=item *

Father Chrysostomos <sprout@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by L</AUTHORS>

This is free software; you can redistribute it and/or modify it under the same terms as the Perl 5 programming language system itself.

=cut

no warnings;
'Danger! Laws of Thermodynamics may not apply.'
