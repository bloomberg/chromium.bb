package Class::C3::Componentised;

=head1 NAME

Class::C3::Componentised - Load mix-ins or components to your C3-based class

=head1 SYNOPSIS

  package MyModule;

  use strict;
  use warnings;

  use base 'Class::C3::Componentised';

  sub component_base_class { "MyModule::Component" }

  package main;

  MyModule->load_components( qw/Foo Bar/ );
  # Will load MyModule::Component::Foo and MyModule::Component::Bar

=head1 DESCRIPTION

This will inject base classes to your module using the L<Class::C3> method
resolution order.

Please note: these are not plugins that can take precedence over methods
declared in MyModule. If you want something like that, consider
L<MooseX::Object::Pluggable>.

=head1 METHODS

=cut

use strict;
use warnings;

# This will prime the Class::C3 namespace (either by loading it proper on 5.8
# or by installing compat shims on 5.10+). A user might have a reasonable
# expectation that using Class::C3::<something> will give him access to
# Class::C3 itself, and this module has been providing this historically.
# Therefore leaving it in indefinitely.
use MRO::Compat;

use Carp ();
use List::Util ();

our $VERSION = '1.001002';
$VERSION =~ tr/_//d;

my $invalid_class = qr/(?: \b:\b | \:{3,} | \:\:$ )/x;

=head2 load_components( @comps )

Loads the given components into the current module. If a module begins with a
C<+> character, it is taken to be a fully qualified class name, otherwise
C<< $class->component_base_class >> is prepended to it.

Calling this will call C<Class::C3::reinitialize>.

=cut

sub load_components {
  my $class = shift;
  $class->_load_components( map {
    /^\+(.*)$/
      ? $1
      : join ('::', $class->component_base_class, $_)
    } grep { $_ !~ /^#/ } @_
  );
}

=head2 load_own_components( @comps )

Similar to L<load_components|/load_components( @comps )>, but assumes every
class is C<"$class::$comp">.

=cut

sub load_own_components {
  my $class = shift;
  $class->_load_components( map { "${class}::$_" } grep { $_ !~ /^#/ } @_ );
}

sub _load_components {
  my $class = shift;
  return unless @_;

  $class->ensure_class_loaded($_) for @_;
  $class->inject_base($class => @_);
  Class::C3::reinitialize();
}

=head2 load_optional_components

As L<load_components|/load_components( @comps )>, but will silently ignore any
components that cannot be found.

=cut

sub load_optional_components {
  my $class = shift;
  $class->_load_components( grep
    { $class->load_optional_class( $_ ) }
    ( map
      { /^\+(.*)$/
          ? $1
          : join ('::', $class->component_base_class, $_)
      }
      grep { $_ !~ /^#/ } @_
    )
  );
}

=head2 ensure_class_loaded

Given a class name, tests to see if it is already loaded or otherwise
defined. If it is not yet loaded, the package is require'd, and an exception
is thrown if the class is still not loaded.

 BUG: For some reason, packages with syntax errors are added to %INC on
      require
=cut

sub ensure_class_loaded {
  my ($class, $f_class) = @_;

  no strict 'refs';

  # ripped from Class::Inspector for speed
  # note that the order is important (faster items are first)
  return if ${"${f_class}::VERSION"};

  return if @{"${f_class}::ISA"};

  my $file = (join ('/', split ('::', $f_class) ) ) . '.pm';
  return if $INC{$file};

  for ( keys %{"${f_class}::"} ) {
    return if ( *{"${f_class}::$_"}{CODE} );
  }

  # require always returns true on success
  # ill-behaved modules might very well obliterate $_
  eval { local $_; require($file) } or do {

    $@ = "Invalid class name '$f_class'" if $f_class =~ $invalid_class;

    if ($class->can('throw_exception')) {
      $class->throw_exception($@);
    } else {
      Carp::croak $@;
    }
  };

  return;
}

=head2 ensure_class_found

Returns true if the specified class is installed or already loaded, false
otherwise.

=cut

sub ensure_class_found {
  #my ($class, $f_class) = @_;
  require Class::Inspector;
  return Class::Inspector->loaded($_[1]) ||
         Class::Inspector->installed($_[1]);
}


=head2 inject_base

Does the actual magic of adjusting C<@ISA> on the target module.

=cut

sub inject_base {
  my $class = shift;
  my $target = shift;

  mro::set_mro($target, 'c3');

  for my $comp (reverse @_) {
    my $apply = do {
      no strict 'refs';
      sub { unshift ( @{"${target}::ISA"}, $comp ) };
    };
    unless ($target eq $comp || $target->isa($comp)) {
      our %APPLICATOR_FOR;
      if (my $apply_class
            = List::Util::first { $APPLICATOR_FOR{$_} } @{mro::get_linear_isa($comp)}
      ) {
        $APPLICATOR_FOR{$apply_class}->_apply_component_to_class($comp,$target,$apply);
      } else {
        $apply->();
      }
    }
  }
}

=head2 load_optional_class

Returns a true value if the specified class is installed and loaded
successfully, throws an exception if the class is found but not loaded
successfully, and false if the class is not installed

=cut

sub load_optional_class {
  my ($class, $f_class) = @_;

  # ensure_class_loaded either returns a () (*not* true)  or throws
  eval {
   $class->ensure_class_loaded($f_class);
   1;
  } && return 1;

  my $err = $@;   # so we don't lose it

  if ($f_class =~ $invalid_class) {
    $err = "Invalid class name '$f_class'";
  }
  else {
    my $fn = quotemeta( (join ('/', split ('::', $f_class) ) ) . '.pm' );
    return 0 if ($err =~ /Can't locate ${fn} in \@INC/ );
  }

  if ($class->can('throw_exception')) {
    $class->throw_exception($err);
  }
  else {
    die $err;
  }
}

=head1 AUTHORS

Matt S. Trout and the L<DBIx::Class team|DBIx::Class/CONTRIBUTORS>

Pulled out into separate module by Ash Berlin C<< <ash@cpan.org> >>

Optimizations and overall bolt-tightening by Peter "ribasushi" Rabbitson
C<< <ribasushi@cpan.org> >>

=head1 COPYRIGHT

Copyright (c) 2006 - 2011 the Class::C3::Componentised L</AUTHORS> as listed
above.

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut

1;
