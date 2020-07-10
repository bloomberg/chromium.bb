package Moose::Meta::Role::Application;
our $VERSION = '2.2011';

use strict;
use warnings;
use metaclass;
use overload ();

use List::Util 1.33 qw( all );

use Moose::Util 'throw_exception';

__PACKAGE__->meta->add_attribute('method_exclusions' => (
    init_arg => '-excludes',
    reader   => 'get_method_exclusions',
    default  => sub { [] },
    Class::MOP::_definition_context(),
));

__PACKAGE__->meta->add_attribute('method_aliases' => (
    init_arg => '-alias',
    reader   => 'get_method_aliases',
    default  => sub { {} },
    Class::MOP::_definition_context(),
));

sub new {
    my ($class, %params) = @_;
    $class->_new(\%params);
}

sub is_method_excluded {
    my ($self, $method_name) = @_;
    foreach (@{$self->get_method_exclusions}) {
        return 1 if $_ eq $method_name;
    }
    return 0;
}

sub is_method_aliased {
    my ($self, $method_name) = @_;
    exists $self->get_method_aliases->{$method_name} ? 1 : 0
}

sub is_aliased_method {
    my ($self, $method_name) = @_;
    my %aliased_names = reverse %{$self->get_method_aliases};
    exists $aliased_names{$method_name} ? 1 : 0;
}

sub apply {
    my $self = shift;

    $self->check_role_exclusions(@_);
    $self->check_required_methods(@_);
    $self->check_required_attributes(@_);

    $self->apply_overloading(@_);
    $self->apply_attributes(@_);
    $self->apply_methods(@_);

    $self->apply_override_method_modifiers(@_);

    $self->apply_before_method_modifiers(@_);
    $self->apply_around_method_modifiers(@_);
    $self->apply_after_method_modifiers(@_);
}

sub check_role_exclusions           { throw_exception( "CannotCallAnAbstractMethod" ); }
sub check_required_methods          { throw_exception( "CannotCallAnAbstractMethod" ); }
sub check_required_attributes       { throw_exception( "CannotCallAnAbstractMethod" ); }

sub apply_attributes                { throw_exception( "CannotCallAnAbstractMethod" ); }
sub apply_methods                   { throw_exception( "CannotCallAnAbstractMethod" ); }
sub apply_override_method_modifiers { throw_exception( "CannotCallAnAbstractMethod" ); }
sub apply_method_modifiers          { throw_exception( "CannotCallAnAbstractMethod" ); }

sub apply_before_method_modifiers   { (shift)->apply_method_modifiers('before' => @_) }
sub apply_around_method_modifiers   { (shift)->apply_method_modifiers('around' => @_) }
sub apply_after_method_modifiers    { (shift)->apply_method_modifiers('after'  => @_) }

sub apply_overloading {
    my ( $self, $role, $other ) = @_;

    return unless $role->is_overloaded;

    unless ( $other->is_overloaded ) {
        $other->set_overload_fallback_value(
            $role->get_overload_fallback_value );
    }

    for my $overload ( $role->get_all_overloaded_operators ) {
        next if $other->has_overloaded_operator( $overload->operator );
        $other->add_overloaded_operator(
            $overload->operator => $overload->clone );
    }
}

1;

# ABSTRACT: A base class for role application

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::Role::Application - A base class for role application

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This is the abstract base class for role applications.

The API for this class and its subclasses still needs some
consideration, and is intentionally not yet documented.

=head2 METHODS

=over 4

=item B<new>

=item B<meta>

=item B<get_method_exclusions>

=item B<is_method_excluded>

=item B<get_method_aliases>

=item B<is_aliased_method>

=item B<is_method_aliased>

=item B<apply>

=item B<check_role_exclusions>

=item B<check_required_methods>

=item B<check_required_attributes>

=item B<apply_attributes>

=item B<apply_methods>

=item B<apply_overloading>

=item B<apply_method_modifiers>

=item B<apply_before_method_modifiers>

=item B<apply_after_method_modifiers>

=item B<apply_around_method_modifiers>

=item B<apply_override_method_modifiers>

=back

=head1 BUGS

See L<Moose/BUGS> for details on reporting bugs.

=head1 AUTHORS

=over 4

=item *

Stevan Little <stevan.little@iinteractive.com>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

Shawn M Moore <code@sartak.org>

=item *

יובל קוג'מן (Yuval Kogman) <nothingmuch@woobling.org>

=item *

Karen Etheridge <ether@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Hans Dieter Pearcey <hdp@weftsoar.net>

=item *

Chris Prather <chris@prather.org>

=item *

Matt S Trout <mst@shadowcat.co.uk>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2006 by Infinity Interactive, Inc.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
