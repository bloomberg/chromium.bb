package MooseX::Meta::TypeConstraint::ForceCoercion;
our $VERSION = '0.01';

# ABSTRACT: Force coercion when validating type constraints

use Moose;
use namespace::autoclean;



has _type_constraint => (
    is       => 'ro',
    isa      => 'Moose::Meta::TypeConstraint',
    init_arg => 'type_constraint',
    required => 1,
);


sub check {
    my ($self, $value) = @_;
    my $coerced = $self->_type_constraint->coerce($value);
    return undef if $coerced == $value;
    return $self->_type_constraint->check($coerced);
}


sub validate {
    my ($self, $value, $coerced_ref) = @_;
    my $coerced = $self->_type_constraint->coerce($value);
    return 'Coercion failed' if $coerced == $value;
    ${ $coerced_ref } = $coerced if $coerced_ref;
    return $self->_type_constraint->validate($coerced);
}

my $meta = __PACKAGE__->meta;

for my $meth (qw/isa can meta/) {
    my $orig = __PACKAGE__->can($meth);
    $meta->add_method($meth => sub {
        my ($self) = shift;
        return $self->$orig(@_) unless blessed $self;

        my $tc = $self->_type_constraint;
        # this might happen during global destruction
        return $self->$orig(@_) unless $tc;

        return $tc->$meth(@_);
    });
}

sub AUTOLOAD {
    my $self = shift;
    my ($meth) = (our $AUTOLOAD =~ /([^:]+)$/);
    return unless blessed $self;

    my $tc = $self->_type_constraint;
    return unless $tc;

    return $tc->$meth(@_);
}

$meta->make_immutable;

1;

__END__
=head1 NAME

MooseX::Meta::TypeConstraint::ForceCoercion - Force coercion when validating type constraints

=head1 VERSION

version 0.01

=head1 SYNOPSIS

    use MooseX::Types:::Moose qw/Str Any/;
    use Moose::Util::TypeConstraints;
    use MooseX::Meta::TypeConstraint::ForceCoercion;

    # get any type constraint
    my $tc = Str;

    # declare one or more coercions for it
    coerce $tc,
        from Any,
        via { ... };

    # wrap the $tc to force coercion
    my $coercing_tc = MooseX::Meta::TypeConstraint::ForceCoercion->new(
        type_constraint => $tc,
    );

    # check a value against new type constraint. this will run the type
    # coercions for the wrapped type, even if the value already passes
    # validation before coercion. it will fail if the value couldn't be
    # coerced
    $coercing_tc->check('Affe');

=head1 DESCRIPTION

This class allows to wrap any C<Moose::Meta::TypeConstraint> in a way that will
force coercion of the value when checking or validating a value against it.



=head1 ATTRIBUTES

=head2 type_constraint

The type constraint to wrap. All methods except for C<validate> and C<check>
are delegated to the value of this attribute.



=head1 METHODS

=head2 check ($value)

Same as C<Moose::Meta::TypeConstraint::check>, except it will always try to
coerce C<$value> before checking it against the actual type constraint. If
coercing fails the check will fail, too.



=head2 validate ($value, $coerced_ref?)

Same as C<Moose::Meta::TypeConstraint::validate>, except it will always try to
coerce C<$value> before validating it against the actual type constraint. If
coercing fails the validation will fail, too.

If coercion was successful and a C<$coerced_ref> references was passed, the
coerced value will be stored in that.

=head1 AUTHOR

  Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2009 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as perl itself.

