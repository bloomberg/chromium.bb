package Class::MOP::Class::Immutable::Trait;
our $VERSION = '2.2011';

use strict;
use warnings;

use MRO::Compat;
use Module::Runtime 'use_module';

# the original class of the metaclass instance
sub _get_mutable_metaclass_name { $_[0]{__immutable}{original_class} }

sub is_mutable   { 0 }
sub is_immutable { 1 }

sub _immutable_metaclass { ref $_[1] }

sub _immutable_read_only {
    my $name = shift;
    __throw_exception( CallingReadOnlyMethodOnAnImmutableInstance => method_name => $name );
}

sub _immutable_cannot_call {
    my $name = shift;
    __throw_exception( CallingMethodOnAnImmutableInstance => method_name => $name );
}

for my $name (qw/superclasses/) {
    no strict 'refs';
    *{__PACKAGE__."::$name"} = sub {
        my $orig = shift;
        my $self = shift;
        _immutable_read_only($name) if @_;
        $self->$orig;
    };
}

for my $name (qw/add_method alias_method remove_method add_attribute remove_attribute remove_package_symbol add_package_symbol/) {
    no strict 'refs';
    *{__PACKAGE__."::$name"} = sub { _immutable_cannot_call($name) };
}

sub class_precedence_list {
    my $orig = shift;
    my $self = shift;
    @{ $self->{__immutable}{class_precedence_list}
            ||= [ $self->$orig ] };
}

sub linearized_isa {
    my $orig = shift;
    my $self = shift;
    @{ $self->{__immutable}{linearized_isa} ||= [ $self->$orig ] };
}

sub get_all_methods {
    my $orig = shift;
    my $self = shift;
    @{ $self->{__immutable}{get_all_methods} ||= [ $self->$orig ] };
}

sub get_all_method_names {
    my $orig = shift;
    my $self = shift;
    @{ $self->{__immutable}{get_all_method_names} ||= [ $self->$orig ] };
}

sub get_all_attributes {
    my $orig = shift;
    my $self = shift;
    @{ $self->{__immutable}{get_all_attributes} ||= [ $self->$orig ] };
}

sub get_meta_instance {
    my $orig = shift;
    my $self = shift;
    $self->{__immutable}{get_meta_instance} ||= $self->$orig;
}

sub _method_map {
    my $orig = shift;
    my $self = shift;
    $self->{__immutable}{_method_map} ||= $self->$orig;
}

# private method, for this file only -
# if we declare a method here, it will behave differently depending on what
# class this trait is applied to, so we won't have a reliable parameter list.
sub __throw_exception {
    my ($exception_type, @args_to_exception) = @_;
    die use_module( "Moose::Exception::$exception_type" )->new( @args_to_exception );
}

1;

# ABSTRACT: Implements immutability for metaclass objects

__END__

=pod

=encoding UTF-8

=head1 NAME

Class::MOP::Class::Immutable::Trait - Implements immutability for metaclass objects

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This class provides a pseudo-trait that is applied to immutable metaclass
objects. In reality, it is simply a parent class.

It implements caching and read-only-ness for various metaclass methods.

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
