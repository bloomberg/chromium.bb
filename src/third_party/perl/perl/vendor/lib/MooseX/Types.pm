package MooseX::Types;
{
  $MooseX::Types::VERSION = '0.31';
}
use Moose;

# ABSTRACT: Organise your Moose types in libraries

use Moose::Util::TypeConstraints;
use MooseX::Types::TypeDecorator;
use MooseX::Types::Base               ();
use MooseX::Types::Util               qw( filter_tags );
use MooseX::Types::UndefinedType;
use MooseX::Types::CheckedUtilExports ();
use Carp::Clan                        qw( ^MooseX::Types );
use Sub::Name;
use Scalar::Util                      'reftype';

use namespace::clean -except => [qw( meta )];

use 5.008;
my $UndefMsg = q{Action for type '%s' not yet defined in library '%s'};


sub import {
    my ($class, %args) = @_;
    my  $callee = caller;

    # everyone should want this
    strict->import;
    warnings->import;

    # inject base class into new library
    {   no strict 'refs';
        unshift @{ $callee . '::ISA' }, 'MooseX::Types::Base';
    }

    # generate predeclared type helpers
    if (my @orig_declare = @{ $args{ -declare } || [] }) {
        my ($tags, $declare) = filter_tags @orig_declare;
        my @to_export;

        for my $type (@$declare) {

            croak "Cannot create a type containing '::' ($type) at the moment"
                if $type =~ /::/;

            # add type to library and remember to export
            $callee->add_type($type);
            push @to_export, $type;
        }

        $callee->import({ -full => 1, -into => $callee }, @to_export);
    }

    # run type constraints import
    Moose::Util::TypeConstraints->import({ into => $callee });

    # override some with versions that check for syntax errors
    MooseX::Types::CheckedUtilExports->import({ into => $callee });

    1;
}


sub type_export_generator {
    my ($class, $type, $name) = @_;
    
    ## Return an anonymous subroutine that will generate the proxied type
    ## constraint for you.

    return subname "__TYPE__::$name" => sub {
        my $type_constraint = $class->create_base_type_constraint($name);

        if(defined(my $params = shift @_)) {
            ## We currently only allow a TC to accept a single, ArrayRef
            ## parameter, as in HashRef[Int], where [Int] is what's inside the
            ## ArrayRef passed.
            if(reftype $params eq 'ARRAY') {
                $type_constraint = $class->create_arged_type_constraint($name, @$params);
            } elsif(!defined $type_constraint) {
                croak "Syntax error in type definition (did you forget a comma"
                    . " after $type?)";
            } else {
                croak "Argument must be an ArrayRef to create a parameterized "
                    . "type, Eg.: ${type}[Int]. Got: ".ref($params)."."
            }
        }

        $type_constraint = defined($type_constraint) ? $type_constraint
         : MooseX::Types::UndefinedType->new($name);
         
        my $type_decorator = $class->create_type_decorator($type_constraint);
        
        ## If there are additional args, that means it's probably stuff that
        ## needs to be returned to the subtype.  Not an ideal solution here but
        ## doesn't seem to cause trouble.
        
        if(@_) {
            return ($type_decorator, @_);
        } else {
            return $type_decorator;
        }
    };
}


sub create_arged_type_constraint {
    my ($class, $name, @args) = @_;  
    my $type_constraint = Moose::Util::TypeConstraints::find_or_create_type_constraint("$name");
    my $parameterized = $type_constraint->parameterize(@args);
    # It's obnoxious to have to parameterize before looking for the TC, but the
    # alternative is to hard-code the assumption that the name is
    # "$name[$args[0]]", which would be worse.
    # This breaks MXMS, unfortunately, which relies on things like Tuple[...]
    # creating new type objects each time.
    # if (my $existing =
    #     Moose::Util::TypeConstraints::find_type_constraint($parameterized->name)) {
    #     return $existing;
    # }
    # Moose::Util::TypeConstraints::register_type_constraint($parameterized);
    return $parameterized;
}


sub create_base_type_constraint {
    my ($class, $name) = @_;
    return find_type_constraint($name);
}


sub create_type_decorator {
    my ($class, $type_constraint) = @_;
    return MooseX::Types::TypeDecorator->new($type_constraint);
}


sub coercion_export_generator {
    my ($class, $type, $full, $undef_msg) = @_;
    return sub {
        my ($value) = @_;

        # we need a type object
        my $tobj = find_type_constraint($full) or croak $undef_msg;
        my $return = $tobj->coerce($value);

        # non-successful coercion returns false
        return unless $tobj->check($return);

        return $return;
    }
}


sub check_export_generator {
    my ($class, $type, $full, $undef_msg) = @_;
    return sub {
        my ($value) = @_;

        # we need a type object
        my $tobj = find_type_constraint($full) or croak $undef_msg;

        return $tobj->check($value);
    }
}


1;

__END__
=pod

=head1 NAME

MooseX::Types - Organise your Moose types in libraries

=head1 VERSION

version 0.31

=head1 SYNOPSIS

=head2 Library Definition

  package MyLibrary;

  # predeclare our own types
  use MooseX::Types -declare => [
      qw(
          PositiveInt
          NegativeInt
          ArrayRefOfPositiveInt
          ArrayRefOfAtLeastThreeNegativeInts
          LotsOfInnerConstraints
          StrOrArrayRef
          MyDateTime
          )
  ];

  # import builtin types
  use MooseX::Types::Moose qw/Int HashRef/;

  # type definition.
  subtype PositiveInt,
      as Int,
      where { $_ > 0 },
      message { "Int is not larger than 0" };

  subtype NegativeInt,
      as Int,
      where { $_ < 0 },
      message { "Int is not smaller than 0" };

  # type coercion
  coerce PositiveInt,
      from Int,
          via { 1 };

  # with parameterized constraints.

  subtype ArrayRefOfPositiveInt,
    as ArrayRef[PositiveInt];

  subtype ArrayRefOfAtLeastThreeNegativeInts,
    as ArrayRef[NegativeInt],
    where { scalar(@$_) > 2 };

  subtype LotsOfInnerConstraints,
    as ArrayRef[ArrayRef[HashRef[Int]]];

  # with TypeConstraint Unions

  subtype StrOrArrayRef,
    as Str|ArrayRef;

  # class types

  class_type 'DateTime';

  # or better

  class_type MyDateTime, { class => 'DateTime' };

  coerce MyDateTime,
    from HashRef,
    via { DateTime->new(%$_) };

  1;

=head2 Usage

  package Foo;
  use Moose;
  use MyLibrary qw( PositiveInt NegativeInt );

  # use the exported constants as type names
  has 'bar',
      isa    => PositiveInt,
      is     => 'rw';
  has 'baz',
      isa    => NegativeInt,
      is     => 'rw';

  sub quux {
      my ($self, $value);

      # test the value
      print "positive\n" if is_PositiveInt($value);
      print "negative\n" if is_NegativeInt($value);

      # coerce the value, NegativeInt doesn't have a coercion
      # helper, since it didn't define any coercions.
      $value = to_PositiveInt($value) or die "Cannot coerce";
  }

  1;

=head1 DESCRIPTION

The type system provided by Moose effectively makes all of its builtin type
global, as are any types you declare with Moose. This means that every module
that declares a type named "PositiveInt" is sharing the same type object. This
can be a problem when different parts of the code base want to use the same
name for different things.

This package lets you declare types using short names, but behind the scenes
it namespaces all your type declarations, effectively prevent name clashes
between packages.

This is done by creating a type library module like C<MyApp::Types> and then
importing types from that module into other modules.

As a side effect, the declaration mechanism allows you to write type names as
barewords (really function calls), which catches typos in names at compile
time rather than run time.

This module also provides some helper functions for using Moose types outside
of attribute declarations.

If you mix string-based names with types created by this module, it will warn,
with a few exceptions. If you are declaring a C<class_type()> or
c<role_type()> within your type library, or if you use a fully qualified name
like C<"MyApp::Foo">.

=head1 LIBRARY DEFINITION

A MooseX::Types is just a normal Perl module. Unlike Moose 
itself, it does not install C<use strict> and C<use warnings> in your
class by default, so this is up to you.

The only thing a library is required to do is

  use MooseX::Types -declare => \@types;

with C<@types> being a list of types you wish to define in this library.
This line will install a proper base class in your package as well as the
full set of L<handlers|/"TYPE HANDLER FUNCTIONS"> for your declared 
types. It will then hand control over to L<Moose::Util::TypeConstraints>'
C<import> method to export the functions you will need to declare your
types.

If you want to use Moose' built-in types (e.g. for subtyping) you will 
want to 

  use MooseX::Types::Moose @types;

to import the helpers from the shipped L<MooseX::Types::Moose>
library which can export all types that come with Moose.

You will have to define coercions for your types or your library won't
export a L</to_$type> coercion helper for it.

Note that you currently cannot define types containing C<::>, since 
exporting would be a problem.

You also don't need to use C<warnings> and C<strict>, since the
definition of a library automatically exports those.

=head1 LIBRARY USAGE

You can import the L<"type helpers"|/"TYPE HANDLER FUNCTIONS"> of a
library by C<use>ing it with a list of types to import as arguments. If
you want all of them, use the C<:all> tag. For example:

  use MyLibrary      ':all';
  use MyOtherLibrary qw( TypeA TypeB );

MooseX::Types comes with a library of Moose' built-in types called
L<MooseX::Types::Moose>.

The exporting mechanism is, since version 0.5, implemented via a wrapper
around L<Sub::Exporter>. This means you can do something like this:

  use MyLibrary TypeA => { -as => 'MyTypeA' },
                TypeB => { -as => 'MyTypeB' };

=head1 TYPE HANDLER FUNCTIONS

=head2 $type

A constant with the name of your type. It contains the type's fully
qualified name. Takes no value, as all constants.

=head2 is_$type

This handler takes a value and tests if it is a valid value for this
C<$type>. It will return true or false.

=head2 to_$type

A handler that will take a value and coerce it into the C<$type>. It will
return a false value if the type could not be coerced.

B<Important Note>: This handler will only be exported for types that can
do type coercion. This has the advantage that a coercion to a type that
has not defined any coercions will lead to a compile-time error.

=head1 WRAPPING A LIBRARY

You can define your own wrapper subclasses to manipulate the behaviour
of a set of library exports. Here is an example:

  package MyWrapper;
  use strict;
  use MRO::Compat;
  use base 'MooseX::Types::Wrapper';

  sub coercion_export_generator {
      my $class = shift;
      my $code = $class->next::method(@_);
      return sub {
          my $value = $code->(@_);
          warn "Coercion returned undef!"
              unless defined $value;
          return $value;
      };
  }

  1;

This class wraps the coercion generator (e.g., C<to_Int()>) and warns
if a coercion returned an undefined value. You can wrap any library
with this:

  package Foo;
  use strict;
  use MyWrapper MyLibrary => [qw( Foo Bar )],
                Moose     => [qw( Str Int )];

  ...
  1;

The C<Moose> library name is a special shortcut for L<MooseX::Types::Moose>.

=head2 Generator methods you can overload

=over 4

=item type_export_generator( $short, $full )

Creates a closure returning the type's L<Moose::Meta::TypeConstraint> object.

=item check_export_generator( $short, $full, $undef_message )

This creates the closure used to test if a value is valid for this type.

=item coercion_export_generator( $short, $full, $undef_message )

This is the closure that's doing coercions.

=back

=head2 Provided Parameters

=over 4

=item $short

The short, exported name of the type.

=item $full

The fully qualified name of this type as L<Moose> knows it.

=item $undef_message

A message that will be thrown when type functionality is used but the
type does not yet exist.

=back

=head1 RECURSIVE SUBTYPES

As of version 0.08, L<Moose::Types> has experimental support for Recursive
subtypes.  This will allow:

    subtype Tree() => as HashRef[Str|Tree];

Which validates things like:

    {key=>'value'};
    {key=>{subkey1=>'value', subkey2=>'value'}}

And so on.  This feature is new and there may be lurking bugs so don't be afraid
to hunt me down with patches and test cases if you have trouble.

=head1 NOTES REGARDING TYPE UNIONS

L<MooseX::Types> uses L<MooseX::Types::TypeDecorator> to do some overloading
which generally allows you to easily create union types:

  subtype StrOrArrayRef,
      as Str|ArrayRef;

As with parameterized constrains, this overloading extends to modules using the
types you define in a type library.

  use Moose;
  use MooseX::Types::Moose qw(HashRef Int);

  has 'attr' => ( isa => HashRef | Int );

And everything should just work as you'd think.

=head1 METHODS

=head2 import

Installs the L<MooseX::Types::Base> class into the caller and exports types
according to the specification described in L</"LIBRARY DEFINITION">. This
will continue to L<Moose::Util::TypeConstraints>' C<import> method to export
helper functions you will need to declare your types.

=head2 type_export_generator

Generate a type export, e.g. C<Int()>. This will return either a
L<Moose::Meta::TypeConstraint> object, or alternatively a
L<MooseX::Types::UndefinedType> object if the type was not yet defined.

=head2 create_arged_type_constraint ($name, @args)

Given a String $name with @args find the matching typeconstraint and parameterize
it with @args.

=head2 create_base_type_constraint ($name)

Given a String $name, find the matching type constraint.

=head2 create_type_decorator ($type_constraint)

Given a $type_constraint, return a lightweight L<MooseX::Types::TypeDecorator>
instance.

=head2 coercion_export_generator

This generates a coercion handler function, e.g. C<to_Int($value)>.

=head2 check_export_generator

Generates a constraint check closure, e.g. C<is_Int($value)>.

=head1 CAVEATS

The following are lists of gotchas and their workarounds for developers coming
from the standard string based type constraint names

=head2 Uniqueness

A library makes the types quasi-unique by prefixing their names with (by
default) the library package name. If you're only using the type handler
functions provided by MooseX::Types, you shouldn't ever have to use
a type's actual full name.

=head2 Argument separation ('=>' versus ',')

The L<perlop> manpage has this to say about the '=>' operator: "The => operator is
a synonym for the comma, but forces any word (consisting entirely of word
characters) to its left to be interpreted as a string (as of 5.001). This
includes words that might otherwise be considered a constant or function call."

Due to this stringification, the following will NOT work as you might think:

  subtype StrOrArrayRef => as Str | ArrayRef;

The 'StrOrArrayRef' will have its stringification activated this causes the
subtype to not be created.  Since the bareword type constraints are not strings
you really should not try to treat them that way.  You will have to use the ','
operator instead.  The author's of this package realize that all the L<Moose>
documention and examples nearly uniformly use the '=>' version of the comma
operator and this could be an issue if you are converting code.

Patches welcome for discussion.

=head2 Compatibility with Sub::Exporter

If you want to use L<Sub::Exporter> with a Type Library, you need to make sure
you export all the type constraints declared AS WELL AS any additional export
targets. For example if you do:

  package TypeAndSubExporter;

  use MooseX::Types::Moose qw(Str);
  use MooseX::Types -declare => [qw(MyStr)];
  use Sub::Exporter -setup => { exports => [qw(something)] };

  subtype MyStr, as Str;

  sub something {
      return 1;
  }

  # then in another module ...

  package Foo;
  use TypeAndSubExporter qw(MyStr);

You'll get a '"MyStr" is not exported by the TypeAndSubExporter module' error.
Upi can workaround by:

  - use Sub::Exporter -setup => { exports => [ qw(something) ] };
  + use Sub::Exporter -setup => { exports => [ qw(something MyStr) ] };

This is a workaround and I am exploring how to make these modules work better
together.  I realize this workaround will lead a lot of duplication in your
export declarations and will be onerous for large type libraries.  Patches and
detailed test cases welcome. See the tests directory for a start on this.

=head1 COMBINING TYPE LIBRARIES

You may want to combine a set of types for your application with other type
libraries, like L<MooseX::Types::Moose> or L<MooseX::Types::Common::String>.

The L<MooseX::Types::Combine> module provides a simple API for combining a set
of type libraries together.

=head1 SEE ALSO

L<Moose>, L<Moose::Util::TypeConstraints>, L<MooseX::Types::Moose>,
L<Sub::Exporter>

=head1 ACKNOWLEDGEMENTS

Many thanks to the C<#moose> cabal on C<irc.perl.org>.

=head1 CONTRIBUTORS

jnapiorkowski: John Napiorkowski <jjnapiork@cpan.org>

caelum: Rafael Kitover <rkitover@cpan.org>

rafl: Florian Ragwitz <rafl@debian.org>

hdp: Hans Dieter Pearcey <hdp@cpan.org>

autarch: Dave Rolsky <autarch@urth.org>

=head1 AUTHOR

Robert "phaylon" Sedlacek <rs@474.at>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Robert "phaylon" Sedlacek.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

