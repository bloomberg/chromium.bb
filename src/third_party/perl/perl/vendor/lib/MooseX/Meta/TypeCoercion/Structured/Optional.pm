package ## Hide from PAUSE
  MooseX::Meta::TypeCoercion::Structured::Optional;

use Moose;
extends 'Moose::Meta::TypeCoercion';


sub compile_type_coercion {
    my ($self) = @_;
    my $constraint = $self->type_constraint->type_parameter;

    $self->_compiled_type_coercion(sub {
        my ($value) = @_;
        return unless $constraint->has_coercion;
        return $constraint->coerce($value);
    });
}

sub has_coercion_for_type { 0 }

sub add_type_coercions {
    Moose->throw_error("Cannot add additional type coercions to Optional types");
}

__PACKAGE__->meta->make_immutable(inline_constructor => 0);

1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Meta::TypeCoercion::Structured::Optional

=head1 METHODS

=head2 compile_type_coercion

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

