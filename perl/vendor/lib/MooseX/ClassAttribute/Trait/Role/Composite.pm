package MooseX::ClassAttribute::Trait::Role::Composite;
BEGIN {
  $MooseX::ClassAttribute::Trait::Role::Composite::VERSION = '0.26';
}

use strict;
use warnings;

use Moose::Util::MetaRole;
use Moose::Util qw(does_role);

use namespace::autoclean;
use Moose::Role;

with 'MooseX::ClassAttribute::Trait::Role';

sub _merge_class_attributes {
    my $self = shift;

    my @all_attributes;
    foreach my $role ( @{ $self->get_roles() } ) {
        if ( does_role( $role, 'MooseX::ClassAttribute::Trait::Role' ) ) {
            push @all_attributes,
                map { $role->get_class_attribute($_) }
                $role->get_class_attribute_list();
        }
    }

    my %seen;

    foreach my $attribute (@all_attributes) {
        my $name = $attribute->name();

        if ( exists $seen{$name} ) {
            next if $seen{$name} == $attribute;

            require Moose;
            Moose->throw_error( "Role '"
                    . $self->name()
                    . "' has encountered a class attribute conflict "
                    . "during composition. This is a fatal error and "
                    . "cannot be disambiguated." );
        }

        $seen{$name} = $attribute;
    }

    foreach my $attribute (@all_attributes) {
        $self->add_class_attribute( $attribute->clone() );
    }

    return keys %seen;
}

around apply_params => sub {
    my $orig = shift;
    my $self = shift;

    $self->$orig(@_);

    $self = Moose::Util::MetaRole::apply_metaroles(
        for            => $self,
        role_metaroles => {
            application_to_class =>
                ['MooseX::ClassAttribute::Trait::Application::ToClass'],
            application_to_role =>
                ['MooseX::ClassAttribute::Trait::Application::ToRole'],
        },
    );

    $self->_merge_class_attributes();

    return $self;
};

1;

# ABSTRACT: A trait that supports applying multiple roles at once



=pod

=head1 NAME

MooseX::ClassAttribute::Trait::Role::Composite - A trait that supports applying multiple roles at once

=head1 VERSION

version 0.26

=head1 DESCRIPTION

This trait is used to allow the application of multiple roles (one
or more of which contain class attributes) to a class or role.

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

