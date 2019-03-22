package MooseX::ClassAttribute::Meta::Role::Attribute;
BEGIN {
  $MooseX::ClassAttribute::Meta::Role::Attribute::VERSION = '0.26';
}

use strict;
use warnings;

use List::MoreUtils qw( uniq );

use namespace::autoclean;
use Moose;

extends 'Moose::Meta::Role::Attribute';

sub new {
    my ( $class, $name, %options ) = @_;

    $options{traits} = [
        uniq( @{ $options{traits} || [] } ),
        'MooseX::ClassAttribute::Trait::Attribute'
    ];

    return $class->SUPER::new( $name, %options );
}

1;

# ABSTRACT: An attribute metaclass for class attributes in roles



=pod

=head1 NAME

MooseX::ClassAttribute::Meta::Role::Attribute - An attribute metaclass for class attributes in roles

=head1 VERSION

version 0.26

=head1 DESCRIPTION

This class overrides L<Moose::Meta::Role::Attribute> to support class
attribute declaration in roles.

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

