package MooseX::Declare::Syntax::EmptyBlockIfMissing;
# ABSTRACT: Handle missing blocks after keywords

our $VERSION = '0.43';

use Moose::Role;
use namespace::autoclean;

#pod =head1 DESCRIPTION
#pod
#pod The L<MooseX::Declare::Syntax::NamespaceHandling> role will require that the
#pod consumer handles the case of non-existent blocks. This role will inject
#pod an empty block with only the generated code parts in it.
#pod
#pod =method handle_missing_block
#pod
#pod   Object->handle_missing_block (Object $context, Str $body, %args)
#pod
#pod This will inject the generated code surrounded by C<{ ... }> into the code
#pod where the keyword was called.
#pod
#pod =cut

sub handle_missing_block {
    my ($self, $ctx, $inject, %args) = @_;

    # default to block with nothing more than the default contents
    $ctx->inject_code_parts_here("{ $inject }");
}

#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<MooseX::Declare>
#pod * L<MooseX::Declare::Syntax::NamespaceHandling>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare::Syntax::EmptyBlockIfMissing - Handle missing blocks after keywords

=head1 VERSION

version 0.43

=head1 DESCRIPTION

The L<MooseX::Declare::Syntax::NamespaceHandling> role will require that the
consumer handles the case of non-existent blocks. This role will inject
an empty block with only the generated code parts in it.

=head1 METHODS

=head2 handle_missing_block

  Object->handle_missing_block (Object $context, Str $body, %args)

This will inject the generated code surrounded by C<{ ... }> into the code
where the keyword was called.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Syntax::NamespaceHandling>

=back

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
