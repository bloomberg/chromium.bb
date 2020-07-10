package ## Hide from PAUSE
 MooseX::Meta::TypeConstraint::Structured;
# ABSTRACT: Structured type constraints

our $VERSION = '0.36';

use Moose;
use Devel::PartialDump;
use MooseX::Meta::TypeCoercion::Structured;
extends 'Moose::Meta::TypeConstraint';


#pod =head1 DESCRIPTION
#pod
#pod A structure is a set of L<Moose::Meta::TypeConstraint> that are 'aggregated' in
#pod such a way as that they are all applied to an incoming list of arguments.  The
#pod idea here is that a Type Constraint could be something like, "An C<Int> followed by
#pod an C<Int> and then a C<Str>" and that this could be done so with a declaration like:
#pod
#pod     Tuple[Int,Int,Str]; ## Example syntax
#pod
#pod So a structure is a list of type constraints (the C<Int,Int,Str> in the above
#pod example) which are intended to function together.
#pod
#pod =attr type_constraints
#pod
#pod A list of L<Moose::Meta::TypeConstraint> objects.
#pod
#pod =cut

has 'type_constraints' => (
    is=>'ro',
    isa=>'Ref',
    predicate=>'has_type_constraints',
);

#pod =attr constraint_generator
#pod
#pod =for stopwords subref
#pod
#pod A subref or closure that contains the way we validate incoming values against
#pod a set of type constraints.
#pod
#pod =cut

has 'constraint_generator' => (
    is=>'ro',
    isa=>'CodeRef',
    predicate=>'has_constraint_generator',
);

has coercion => (
    is      => 'ro',
    isa     => 'Object',
    builder => '_build_coercion',
);

sub _build_coercion {
    my ($self) = @_;
    return MooseX::Meta::TypeCoercion::Structured->new(
        type_constraint => $self,
    );
}

#pod =method validate
#pod
#pod Messing with validate so that we can support nicer error messages.
#pod
#pod =cut

sub _clean_message {
    my $message = shift @_;
    $message =~s/MooseX::Types::Structured:://g;
    return $message;
}

override 'validate' => sub {
    my ($self, $value, $message_stack) = @_;
    unless ($message_stack) {
        $message_stack = MooseX::Types::Structured::MessageStack->new();
    }

    $message_stack->inc_level;

    if ($self->_compiled_type_constraint->($value, $message_stack)) {
        ## Everything is good, no error message to return
        return undef;
    } else {
        ## Whoops, need to figure out the right error message
        my $args = Devel::PartialDump::dump($value);
        $message_stack->dec_level;
        if($message_stack->has_messages) {
            if($message_stack->level) {
                ## we are inside a deeply structured constraint
                return $self->get_message($args);
            } else {
                my $message_str = $message_stack->as_string;
                return _clean_message($self->get_message("$args, Internal Validation Error is: $message_str"));
            }
        } else {
            return $self->get_message($args);
        }
    }
};

#pod =method generate_constraint_for ($type_constraints)
#pod
#pod Given some type constraints, use them to generate validation rules for an ref
#pod of values (to be passed at check time)
#pod
#pod =cut

sub generate_constraint_for {
    my ($self, $type_constraints) = @_;
    return $self->constraint_generator->($self, $type_constraints);
}

#pod =for :prelude
#pod =for stopwords parameterize
#pod
#pod =method parameterize (@type_constraints)
#pod
#pod Given a ref of type constraints, create a structured type.
#pod
#pod =cut

sub parameterize {
    my ($self, @type_constraints) = @_;
    my $class = ref $self;
    my $name = $self->name .'['. join(',', map {"$_"} @type_constraints) .']';
    my $constraint_generator = $self->__infer_constraint_generator;

    return $class->new(
        name => $name,
        parent => $self,
        type_constraints => \@type_constraints,
        constraint_generator => $constraint_generator,
    );
}

#pod =method __infer_constraint_generator
#pod
#pod =for stopwords servicable
#pod
#pod This returns a CODEREF which generates a suitable constraint generator.  Not
#pod user servicable, you'll never call this directly.
#pod
#pod =cut

sub __infer_constraint_generator {
    my ($self) = @_;
    if($self->has_constraint_generator) {
        return $self->constraint_generator;
    } else {
        return sub {
            ## I'm not sure about this stuff but everything seems to work
            my $tc = shift @_;
            my $merged_tc = [@$tc, @{$self->parent->type_constraints}];
            $self->constraint->($merged_tc, @_);
        };
    }
}

#pod =method compile_type_constraint
#pod
#pod hook into compile_type_constraint so we can set the correct validation rules.
#pod
#pod =cut

around 'compile_type_constraint' => sub {
    my ($compile_type_constraint, $self, @args) = @_;

    if($self->has_type_constraints) {
        my $type_constraints = $self->type_constraints;
        my $constraint = $self->generate_constraint_for($type_constraints);
        $self->_set_constraint($constraint);
    }

    return $self->$compile_type_constraint(@args);
};

#pod =method create_child_type
#pod
#pod modifier to make sure we get the constraint_generator
#pod
#pod =cut

around 'create_child_type' => sub {
    my ($create_child_type, $self, %opts) = @_;
    return $self->$create_child_type(
        %opts,
        constraint_generator => $self->__infer_constraint_generator,
    );
};

#pod =method is_a_type_of
#pod
#pod =method is_subtype_of
#pod
#pod =method equals
#pod
#pod Override the base class behavior.
#pod
#pod =cut

sub equals {
    my ( $self, $type_or_name ) = @_;
    my $other = Moose::Util::TypeConstraints::find_type_constraint($type_or_name)
      or return;

    return unless $other->isa(__PACKAGE__);

    return (
        $self->parent->equals($other->parent)
            and
        $self->type_constraints_equals($other)
    );
}

sub is_a_type_of {
    my ( $self, $type_or_name ) = @_;
    my $other = Moose::Util::TypeConstraints::find_type_constraint($type_or_name)
      or return;

    if ( $other->isa(__PACKAGE__) and @{ $other->type_constraints || [] }) {
        if ( $self->parent->is_a_type_of($other->parent) ) {
            return $self->_type_constraints_op_all($other, "is_a_type_of");
        } elsif ( $self->parent->is_a_type_of($other) ) {
            return 1;
            # FIXME compare?
        } else {
            return 0;
        }
    } else {
        return $self->SUPER::is_a_type_of($other);
    }
}

sub is_subtype_of {
    my ( $self, $type_or_name ) = @_;
    my $other = Moose::Util::TypeConstraints::find_type_constraint($type_or_name)
      or return;
    if ( $other->isa(__PACKAGE__) ) {
        if ( $other->type_constraints and $self->type_constraints ) {
            if ( $self->parent->is_a_type_of($other->parent) ) {
                return (
                    $self->_type_constraints_op_all($other, "is_a_type_of")
                      and
                    $self->_type_constraints_op_any($other, "is_subtype_of")
                );
            } elsif ( $self->parent->is_a_type_of($other) ) {
                return 1;
                # FIXME compare?
            } else {
                return 0;
            }
        } else {
            if ( $self->type_constraints ) {
                if ( $self->SUPER::is_subtype_of($other) ) {
                    return 1;
                } else {
                    return;
                }
            } else {
                return $self->parent->is_subtype_of($other->parent);
            }
        }
    } else {
        return $self->SUPER::is_subtype_of($other);
    }
}

#pod =method type_constraints_equals
#pod
#pod Checks to see if the internal type constraints are equal.
#pod
#pod =cut

sub type_constraints_equals {
    my ( $self, $other ) = @_;
    $self->_type_constraints_op_all($other, "equals");
}

sub _type_constraints_op_all {
    my ($self, $other, $op) = @_;

    return unless $other->isa(__PACKAGE__);

    my @self_type_constraints = @{$self->type_constraints||[]};
    my @other_type_constraints = @{$other->type_constraints||[]};

    return unless @self_type_constraints == @other_type_constraints;

    ## Incoming ay be either arrayref or hashref, need top compare both
    while(@self_type_constraints) {
        my $self_type_constraint = shift @self_type_constraints;
        my $other_type_constraint = shift @other_type_constraints;

        $_ = Moose::Util::TypeConstraints::find_or_create_isa_type_constraint($_)
          for $self_type_constraint, $other_type_constraint;

        my $result = $self_type_constraint->$op($other_type_constraint);
        return unless $result;
    }

    return 1; ##If we get this far, everything is good.
}

sub _type_constraints_op_any {
    my ($self, $other, $op) = @_;

    return unless $other->isa(__PACKAGE__);

    my @self_type_constraints = @{$self->type_constraints||[]};
    my @other_type_constraints = @{$other->type_constraints||[]};

    return unless @self_type_constraints == @other_type_constraints;

    ## Incoming ay be either arrayref or hashref, need top compare both
    while(@self_type_constraints) {
        my $self_type_constraint = shift @self_type_constraints;
        my $other_type_constraint = shift @other_type_constraints;

        $_ = Moose::Util::TypeConstraints::find_or_create_isa_type_constraint($_)
          for $self_type_constraint, $other_type_constraint;

        return 1 if $self_type_constraint->$op($other_type_constraint);
    }

    return 0;
}

#pod =method get_message
#pod
#pod Give you a better peek into what's causing the error.  For now we stringify the
#pod incoming deep value with L<Devel::PartialDump> and pass that on to either your
#pod custom error message or the default one.  In the future we'll try to provide a
#pod more complete stack trace of the actual offending elements
#pod
#pod =cut

around 'get_message' => sub {
    my ($get_message, $self, $value) = @_;
    $value = Devel::PartialDump::dump($value)
     if ref $value;
    return $self->$get_message($value);
};

#pod =head1 SEE ALSO
#pod
#pod The following modules or resources may be of interest.
#pod
#pod L<Moose>, L<Moose::Meta::TypeConstraint>
#pod
#pod =cut

no Moose;
__PACKAGE__->meta->make_immutable(inline_constructor => 0);

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Meta::TypeConstraint::Structured - Structured type constraints

=head1 VERSION

version 0.36

=for stopwords parameterize

=head1 DESCRIPTION

A structure is a set of L<Moose::Meta::TypeConstraint> that are 'aggregated' in
such a way as that they are all applied to an incoming list of arguments.  The
idea here is that a Type Constraint could be something like, "An C<Int> followed by
an C<Int> and then a C<Str>" and that this could be done so with a declaration like:

    Tuple[Int,Int,Str]; ## Example syntax

So a structure is a list of type constraints (the C<Int,Int,Str> in the above
example) which are intended to function together.

=head1 ATTRIBUTES

=head2 type_constraints

A list of L<Moose::Meta::TypeConstraint> objects.

=head2 constraint_generator

=head1 METHODS

=head2 validate

Messing with validate so that we can support nicer error messages.

=head2 generate_constraint_for ($type_constraints)

Given some type constraints, use them to generate validation rules for an ref
of values (to be passed at check time)

=head2 parameterize (@type_constraints)

Given a ref of type constraints, create a structured type.

=head2 __infer_constraint_generator

=head2 compile_type_constraint

hook into compile_type_constraint so we can set the correct validation rules.

=head2 create_child_type

modifier to make sure we get the constraint_generator

=head2 is_a_type_of

=head2 is_subtype_of

=head2 equals

Override the base class behavior.

=head2 type_constraints_equals

Checks to see if the internal type constraints are equal.

=head2 get_message

Give you a better peek into what's causing the error.  For now we stringify the
incoming deep value with L<Devel::PartialDump> and pass that on to either your
custom error message or the default one.  In the future we'll try to provide a
more complete stack trace of the actual offending elements

=for stopwords subref

A subref or closure that contains the way we validate incoming values against
a set of type constraints.

=for stopwords servicable

This returns a CODEREF which generates a suitable constraint generator.  Not
user servicable, you'll never call this directly.

=head1 SEE ALSO

The following modules or resources may be of interest.

L<Moose>, L<Moose::Meta::TypeConstraint>

=head1 SUPPORT

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=MooseX-Types-Structured>
(or L<bug-MooseX-Types-Structured@rt.cpan.org|mailto:bug-MooseX-Types-Structured@rt.cpan.org>).

There is also a mailing list available for users of this distribution, at
L<http://lists.perl.org/list/moose.html>.

There is also an irc channel available for users of this distribution, at
L<C<#moose> on C<irc.perl.org>|irc://irc.perl.org/#moose>.

=head1 AUTHORS

=over 4

=item *

John Napiorkowski <jjnapiork@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

יובל קוג'מן (Yuval Kogman) <nothingmuch@woobling.org>

=item *

Tomas (t0m) Doran <bobtfish@bobtfish.net>

=item *

Robert Sedlacek <rs@474.at>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by John Napiorkowski.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
