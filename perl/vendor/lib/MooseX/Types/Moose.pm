package MooseX::Types::Moose;
{
  $MooseX::Types::Moose::VERSION = '0.31';
}

# ABSTRACT: Type exports that match the types shipped with L<Moose>

use warnings;
use strict;

use MooseX::Types;
use Moose::Util::TypeConstraints ();

use namespace::clean -except => [qw( meta )];


# all available builtin types as short and long name
my %BuiltIn_Storage 
  = map { ($_) x 2 } 
    Moose::Util::TypeConstraints->list_all_builtin_type_constraints;


# use prepopulated builtin hash as type storage
sub type_storage { \%BuiltIn_Storage }


1;

__END__
=pod

=head1 NAME

MooseX::Types::Moose - Type exports that match the types shipped with L<Moose>

=head1 VERSION

version 0.31

=head1 SYNOPSIS

  package Foo;
  use Moose;
  use MooseX::Types::Moose qw( ArrayRef Int Str );
  use Carp qw( croak );

  has 'name',
    is  => 'rw',
    isa => Str;

  has 'ids',
    is  => 'rw',
    isa => ArrayRef[Int];

  sub add {
      my ($self, $x, $y) = @_;
      croak 'First arg not an Int'  unless is_Int($x);
      croak 'Second arg not an Int' unless is_Int($y);
      return $x + $y;
  }

  1;

=head1 DESCRIPTION

This package contains a virtual library for L<MooseX::Types> that
is able to export all types known to L<Moose>. See L<MooseX::Types>
for general usage information.

=head1 METHODS

=head2 type_storage

Overrides L<MooseX::Types::Base>' C<type_storage> to provide a hash
reference containing all built-in L<Moose> types.

=head1 SEE ALSO

L<MooseX::Types::Moose>,
L<Moose>, 
L<Moose::Util::TypeConstraints>

=head1 LICENSE

This program is free software; you can redistribute it and/or modify
it under the same terms as perl itself.

=head1 AUTHOR

Robert "phaylon" Sedlacek <rs@474.at>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Robert "phaylon" Sedlacek.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

