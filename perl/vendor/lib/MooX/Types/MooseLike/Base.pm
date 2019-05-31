package MooX::Types::MooseLike::Base;
use strict;
use warnings FATAL => 'all';
use Scalar::Util qw(blessed);
use List::Util;
use MooX::Types::MooseLike qw( exception_message inflate_type );
use Exporter 5.57 'import';
our @EXPORT_OK = ();

our $VERSION = 0.29;

# These types act like those found in Moose::Util::TypeConstraints.
# Generally speaking, the same test is used.
sub some_basic_type_definitions {
  return
    (
    {
      name => 'Any',
      test => sub { 1 },
      message =>
        sub { "If you get here you've achieved the impossible, congrats." }
    },
    {
      name => 'Item',
      test => sub { 1 },
      message =>
        sub { "If you get here you've achieved the impossible, congrats" }
    },
    {
      name => 'Bool',

      #  	test    => sub { $_[0] == 0 || $_[0] == 1 },
      test => sub {
        !defined($_[0]) || $_[0] eq "" || "$_[0]" eq '1' || "$_[0]" eq '0';
        },
      message => sub { return exception_message($_[0], 'a Boolean') },
    },

    # Maybe has no test for itself, rather only the parameter type does
    {
      name    => 'Maybe',
      test    => sub { 1 },
      message => sub { 'Maybe only uses its parameterized type message' },
      parameterizable => sub { return if (not defined $_[0]); $_[0] },
    },
    {
      name    => 'Undef',
      test    => sub { !defined($_[0]) },
      message => sub { return exception_message($_[0], 'undef') },
    },
    );
}

sub defined_type_definitions {
  return
    ({
      name    => 'Defined',
      test    => sub { defined($_[0]) },
      message => sub { return exception_message($_[0], 'defined') },
    },
    {
      name    => 'Value',
      test    => sub { defined $_[0] and not ref($_[0]) },
      message => sub { return exception_message($_[0], 'a value') },
    },
    {
      name => 'Str',
      test => sub { defined $_[0] and (ref(\$_[0]) eq 'SCALAR') },
      message => sub { return exception_message($_[0], 'a string') },
    },
    {
      name    => 'Num',
      test    => sub {
          my $val = $_[0];
          defined $val and
        ($val =~ /\A[+-]?[0-9]+\z/) ||
        ( $val =~ /\A(?:[+-]?)            # matches optional +- in the beginning
          (?=[0-9]|\.[0-9])               # matches previous +- only if there is something like 3 or .3
          [0-9]*                          # matches 0-9 zero or more times
          (?:\.[0-9]+)?                   # matches optional .89 or nothing
          (?:[Ee](?:[+-]?[0-9]+))?        # matches E1 or e1 or e-1 or e+1 etc
          \z/x );
      },
      message => sub {
        my $nbr = shift;
        if (not defined $nbr) {
          $nbr = 'undef';
        }
        elsif (not (length $nbr)) {
          $nbr = 'The empty string';
        }
        return exception_message($nbr, 'a number');
        },
    },
    {
      name    => 'Int',
      test    => sub { defined $_[0] and ("$_[0]" =~ /^-?[0-9]+$/x) },
      message => sub {
        my $nbr = shift;
        if (not defined $nbr) {
          $nbr = 'undef';
        }
        elsif (not (length $nbr)) {
          $nbr = 'The empty string';
        }
        return exception_message($nbr, 'an integer');
        },
    },
    );
}

sub ref_type_definitions {
  return
    (
    {
      name    => 'Ref',
      test    => sub { defined $_[0] and ref($_[0]) },
      message => sub { return exception_message($_[0], 'a reference') },
    },

    {
      name => 'ScalarRef',
      test => sub { defined $_[0] and ref($_[0]) eq 'SCALAR' },
      message => sub { return exception_message($_[0], 'a ScalarRef') },
      parameterizable => sub { ${ $_[0] } },
      inflate => sub {
        require Moose::Util::TypeConstraints;
        if (my $params = shift) {
          return Moose::Util::TypeConstraints::_create_parameterized_type_constraint(
            Moose::Util::TypeConstraints::find_type_constraint('ScalarRef'),
            inflate_type(@$params),
          );
        }
        return Moose::Util::TypeConstraints::find_type_constraint('ScalarRef');
        },
    },
    {
      name => 'ArrayRef',
      test => sub { defined $_[0] and ref($_[0]) eq 'ARRAY' },
      message => sub { return exception_message($_[0], 'an ArrayRef') },
      parameterizable => sub { @{ $_[0] } },
      inflate => sub {
        require Moose::Util::TypeConstraints;
        if (my $params = shift) {
          return Moose::Util::TypeConstraints::_create_parameterized_type_constraint(
            Moose::Util::TypeConstraints::find_type_constraint('ArrayRef'),
            inflate_type(@$params),
          );
        }
        return Moose::Util::TypeConstraints::find_type_constraint('ArrayRef');
        },
    },
    {
      name => 'HashRef',
      test => sub { defined $_[0] and ref($_[0]) eq 'HASH' },
      message => sub { return exception_message($_[0], 'a HashRef') },
      parameterizable => sub { values %{ $_[0] } },
      inflate => sub {
        require Moose::Util::TypeConstraints;
        if (my $params = shift) {
          return Moose::Util::TypeConstraints::_create_parameterized_type_constraint(
            Moose::Util::TypeConstraints::find_type_constraint('HashRef'),
            inflate_type(@$params),
          );
        }
        return Moose::Util::TypeConstraints::find_type_constraint('HashRef');
        },
    },
    {
      name => 'CodeRef',
      test => sub { defined $_[0] and ref($_[0]) eq 'CODE' },
      message => sub { return exception_message($_[0], 'a CodeRef') },
    },
    {
      name => 'RegexpRef',
      test => sub { defined $_[0] and ref($_[0]) eq 'Regexp' },
      message => sub { return exception_message($_[0], 'a RegexpRef') },
    },
    {
      name => 'GlobRef',
      test => sub { defined $_[0] and ref($_[0]) eq 'GLOB' },
      message => sub { return exception_message($_[0], 'a GlobRef') },
    },
    );
}

sub filehandle_type_definitions {
  return
    (
    {
      name => 'FileHandle',
      test => sub {
        defined $_[0]
          and Scalar::Util::openhandle($_[0])
          or (blessed($_[0]) && $_[0]->isa("IO::Handle"));
        },
      message => sub { return exception_message($_[0], 'a FileHandle') },
    },
    );
}

sub blessed_type_definitions {## no critic qw(Subroutines::ProhibitExcessComplexity)
  return
    (
    {
      name => 'Object',
      test => sub { defined $_[0] and blessed($_[0]) and blessed($_[0]) ne 'Regexp' },
      message => sub { return exception_message($_[0], 'an Object') },
    },
    {
      name => 'InstanceOf',
      test => sub {
        my ($instance, @classes) = (shift, @_);
        return if not defined $instance;
        return if not blessed($instance);
        my @missing_classes = grep { !$instance->isa($_) } @classes;
        return (scalar @missing_classes ? 0 : 1);
        },
      message => sub {
        my $instance = shift;
        return "No instance given" if not defined $instance;
        return "$instance is not blessed" if not blessed($instance);
        my @missing_classes = grep { !$instance->isa($_) } @_;
        my $s = (scalar @missing_classes) > 1 ? 'es' : '';
        my $missing_classes = join ' ', @missing_classes;
        return "$instance is not an instance of the class${s}: $missing_classes";
        },
      inflate => sub {
        require Moose::Meta::TypeConstraint::Class;
        if (my $classes = shift) {
          if (@$classes == 1) {
            return Moose::Meta::TypeConstraint::Class->new(class => @$classes);
          }
          elsif (@$classes > 1) {
            return Moose::Meta::TypeConstraint->new(
              parent     => Moose::Util::TypeConstraints::find_type_constraint('Object'),
              constraint => sub {
                  my $instance = shift;
                  my @missing_classes = grep { !$instance->isa($_) } @$classes;
                  return (scalar @missing_classes ? 0 : 1);
                },
            );
          }
        }
        return Moose::Util::TypeConstraints::find_type_constraint('Object');
      },
    },
    {
      name => 'ConsumerOf',
      test => sub {
        my ($instance, @roles) = (shift, @_);
        return if not defined $instance;
        return if not blessed($instance);
        return if (!$instance->can('does'));
        my @missing_roles = grep { !$instance->does($_) } @roles;
        return (scalar @missing_roles ? 0 : 1);
        },
      message => sub {
        my $instance = shift;
        return "No instance given" if not defined $instance;
        return "$instance is not blessed" if not blessed($instance);
        return "$instance is not a consumer of roles" if (!$instance->can('does'));
        my @missing_roles = grep { !$instance->does($_) } @_;
        my $s = (scalar @missing_roles) > 1 ? 's' : '';
        my $missing_roles = join ' ', @missing_roles;
        return "$instance does not consume the required role${s}: $missing_roles";
        },
      inflate => sub {
        require Moose::Meta::TypeConstraint::Role;
        if (my $roles = shift) {
          if (@$roles == 1) {
            return Moose::Meta::TypeConstraint::Role->new(role => @$roles);
          }
          elsif (@$roles > 1) {
            return Moose::Meta::TypeConstraint->new(
              parent     => Moose::Util::TypeConstraints::find_type_constraint('Object'),
              constraint => sub {
                  my $instance = shift;
                  return if (!$instance->can('does'));
                  my @missing_roles = grep { !$instance->does($_) } @$roles;
                  return (scalar @missing_roles ? 0 : 1);
                },
            );
          }
        }
        return Moose::Util::TypeConstraints::find_type_constraint('Object');
      },
    },
    {
      name => 'HasMethods',
      test => sub {
        my ($instance, @methods) = (shift, @_);
        return if not defined $instance;
        return if not blessed($instance);
        my @missing_methods = grep { !$instance->can($_) } @methods;
        return (scalar @missing_methods ? 0 : 1);
        },
      message => sub {
        my $instance = shift;
        return "No instance given" if not defined $instance;
        return "$instance is not blessed" if not blessed($instance);
        my @missing_methods = grep { !$instance->can($_) } @_;
        my $s = (scalar @missing_methods) > 1 ? 's' : '';
        my $missing_methods = join ' ', @missing_methods;
        return "$instance does not have the required method${s}: $missing_methods";
        },
      inflate => sub {
        require Moose::Meta::TypeConstraint::DuckType;
        if (my $methods = shift) {
          return Moose::Meta::TypeConstraint::DuckType->new(methods => $methods);
        }
        return Moose::Util::TypeConstraints::find_type_constraint('Object');
      },
    },
    {
      name => 'Enum',
      test => sub {
        my ($value, @possible_values) = @_;
        return if not defined $value;
        return List::Util::first { $value eq $_ } @possible_values;
        },
      message => sub {
        my ($value, @possible_values) = @_;
        my $possible_values = join(', ', @possible_values);
        return exception_message($value, "any of the possible values: ${possible_values}");
      },
      inflate => sub {
        require Moose::Meta::TypeConstraint::Enum;
        if (my $possible_values = shift) {
          return Moose::Meta::TypeConstraint::Enum->new(values => $possible_values);
        }
        die "Enum cannot be inflated to a Moose type without any possible values";
      },
    },
    );
}

sub logic_type_definitions {
  return
    (
    {
      name => 'AnyOf',
      test => sub {
        my ($value, @types) = @_;
        foreach my $type (@types) {
          return 1 if (eval {$type->($value); 1;});
        }
        return;
        },
      message => sub { return exception_message($_[0], 'any of the types') },
      inflate => sub {
        require Moose::Meta::TypeConstraint::Union;
        if (my $types = shift) {
          return Moose::Meta::TypeConstraint::Union->new(
            type_constraints => [ map inflate_type($_), @$types ],
          );
        }
        die "AnyOf cannot be inflated to a Moose type without any possible types";
        },
    },
    {
      name => 'AllOf',
      test => sub { return 1; },
      message => sub { 'AllOf only uses its parameterized type messages' },
      parameterizable => sub { $_[0] },
      inflate => 0,
    },
    );
}

sub type_definitions {
  return
    [
    some_basic_type_definitions()
    ,defined_type_definitions()
    ,ref_type_definitions()
    ,filehandle_type_definitions()
    ,blessed_type_definitions()
    ,logic_type_definitions()
    ];
}

MooX::Types::MooseLike::register_types(type_definitions(), __PACKAGE__);

# Export an 'all' tag so one can easily import all types like so:
# use MooX::Types::MooseLike::Base qw(:all)
our %EXPORT_TAGS = ('all' => \@EXPORT_OK);

1;

__END__

=head1 NAME

MooX::Types::MooseLike::Base - A set of basic Moose-like types for Moo

=head1 SYNOPSIS

  package MyPackage;
  use Moo;
  use MooX::Types::MooseLike::Base qw(:all);

  has "beers_by_day_of_week" => (
      isa => HashRef
  );

  has "current_BAC" => (
      isa => Num
  );

  # Also supporting is_$type.  For example, is_Int() can be used as follows
  has 'legal_age' => (
      is => 'ro',
      isa => sub { die "$_[0] is not of legal age"
      	           unless (is_Int($_[0]) && $_[0] > 17) },
  );

=head1 DESCRIPTION

Moo attributes (like Moose) have an 'isa' property.
This module provides some basic types for this property.
One can import all types with ':all' tag or import
a list of types like:

  use MooX::Types::MooseLike::Base qw/HashRef ArrayRef/;

so one could then declare some attributes like:

  has 'contact' => (
    is => 'ro',
    isa => HashRef,
  );
  has 'guest_list' => (
    is => 'ro',
    isa => ArrayRef[HashRef],
  );

These types provide a check that the I<contact> attribute is a C<hash> reference,
and that the I<guest_list> is an C<array of hash> references.

=head1 TYPES (1st class functions - return a coderef)

=head2 Any

Any type (test is always true)

=head2 Item

Synonymous with Any type

=head2 Undef

A type that is not defined

=head2 Defined

A type that is defined

=head2 Bool

A boolean 1|0 type

=head2 Value

A non-reference type

=head2 Ref

A reference type

=head2 Str

A non-reference type where a reference to it is a SCALAR

=head2 Num

A number type

=head2 Int

An integer type

=head2 ArrayRef

An ArrayRef (ARRAY) type

=head2 HashRef

A HashRef (HASH) type

=head2 CodeRef

A CodeRef (CODE) type

=head2 RegexpRef

A regular expression reference type

=head2 GlobRef

A glob reference type

=head2 FileHandle

A type that is either a builtin perl filehandle or an IO::Handle object

=head2 Object

A type that is an object (think blessed)

=head1 PARAMETERIZED TYPES

=head2 Parameterizing Types With a Single Type

The following types can be parameterized with other types.

=head3 ArrayRef

For example, ArrayRef[HashRef]

=head3 HashRef

=head3 ScalarRef

=head3 Maybe

For example, Maybe[Int] would be an integer or undef

=head2 Parameterizing Types With Multiple Types

=head3 AnyOf

Check if the attribute is any of the listed types (think union).
Takes a list of types as the argument, for example:

  isa => AnyOf[Int, ArrayRef[Int], HashRef[Int]]

Note: AnyOf is passed an ArrayRef[CodeRef]

=head3 AllOf

Check if the attribute is all of the listed types (think intersection).
Takes a list of types as the argument. For example:

  isa => AllOf[
    InstanceOf['Human'],
    ConsumerOf['Air'],
    HasMethods['breath', 'dance']
  ],

=head2 Parameterizing Types With (Multiple) Strings

In addition, we have some parameterized types that take string arguments.

=head3 InstanceOf

Check if the attribute is an object instance of one or more classes.
Uses C<blessed> and C<isa> to do so.
Takes a list of class names as the argument. For example:

  isa => InstanceOf['MyClass','MyOtherClass']

Note: InstanceOf is passed an ArrayRef[Str]

=head3 ConsumerOf

Check if the attribute is blessed and consumes one or more roles.
Uses C<blessed> and C<does> to do so.
Takes a list of role names as the arguments. For example:

  isa => ConsumerOf['My::Role', 'My::AnotherRole']

=head3 HasMethods

Check if the attribute is blessed and has one or more methods.
Uses C<blessed> and C<can> to do so.
Takes a list of method names as the arguments. For example:

  isa => HasMethods[qw/postulate contemplate liberate/]

=head3 Enum

Check if the attribute is one of the enumerated strings.
Takes a list of possible string values. For example:

  isa => Enum['rock', 'spock', 'paper', 'lizard', 'scissors']

=head1 SEE ALSO

L<MooX::Types::MooseLike::Numeric> - an example of building subtypes.

L<MooX::Types::SetObject> - an example of building parameterized types.

L<MooX::Types::MooseLike::Email>, L<MooX::Types::MooseLike::DateTime>

=head1 AUTHOR

Mateu Hunter C<hunter@missoula.org>

=head1 THANKS

mst has provided critical guidance on the design

=head1 COPYRIGHT

Copyright 2011-2015 Mateu Hunter

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
