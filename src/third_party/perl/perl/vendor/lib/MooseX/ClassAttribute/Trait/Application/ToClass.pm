package MooseX::ClassAttribute::Trait::Application::ToClass;
BEGIN {
  $MooseX::ClassAttribute::Trait::Application::ToClass::VERSION = '0.26';
}

use strict;
use warnings;

use namespace::autoclean;
use Moose::Role;

with 'MooseX::ClassAttribute::Trait::Application';

around apply => sub {
    my $orig = shift;
    my $self  = shift;
    my $role  = shift;
    my $class = shift;

    $class = Moose::Util::MetaRole::apply_metaroles(
        for             => $class,
        class_metaroles => {
            class => ['MooseX::ClassAttribute::Trait::Class'],
        },
    );

    $self->$orig( $role, $class );
};

sub _apply_class_attributes {
    my $self  = shift;
    my $role  = shift;
    my $class = shift;

    my $attr_metaclass = $class->attribute_metaclass();

    foreach my $attribute_name ( $role->get_class_attribute_list() ) {
        if (   $class->has_class_attribute($attribute_name)
            && $class->get_class_attribute($attribute_name)
            != $role->get_class_attribute($attribute_name) ) {
            next;
        }
        else {
            $class->add_class_attribute(
                $role->get_class_attribute($attribute_name)
                    ->attribute_for_class($attr_metaclass) );
        }
    }
}

1;

# ABSTRACT: A trait that supports applying class attributes to classes



=pod

=head1 NAME

MooseX::ClassAttribute::Trait::Application::ToClass - A trait that supports applying class attributes to classes

=head1 VERSION

version 0.26

=head1 DESCRIPTION

This trait is used to allow the application of roles containing class
attributes to classes.

=head1 BUGS

See L<MooseX::ClassAttribute> for details.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2011 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut


__END__

