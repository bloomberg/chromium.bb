package ## Hide from PAUSE
 MooseX::Meta::TypeCoercion::Structured;
# ABSTRACT: MooseX::Meta::TypeCoercion::Structured - Coerce structured type constraints.

use Moose;
extends 'Moose::Meta::TypeCoercion';


__PACKAGE__->meta->make_immutable(inline_constructor => 0);

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Meta::TypeCoercion::Structured - MooseX::Meta::TypeCoercion::Structured - Coerce structured type constraints.

=head1 DESCRIPTION

We need to make sure we can properly coerce the structure elements inside a
structured type constraint.  However requirements for the best way to allow
this are still in flux.  For now this class is a placeholder.

=head1 SEE ALSO

The following modules or resources may be of interest.

L<Moose>, L<Moose::Meta::TypeCoercion>

=head1 AUTHORS

=over 4

=item *

John Napiorkowski <jjnapiork@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Yuval Kogman <nothingmuch@woobling.org>

=item *

Tomas Doran <bobtfish@bobtfish.net>

=item *

Robert Sedlacek <rs@474.at>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by John Napiorkowski.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

