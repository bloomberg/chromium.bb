use strict;
use warnings;

package MooseX::Declare::Util;
# ABSTRACT: Common declarative utility functions

our $VERSION = '0.43';

use Sub::Exporter -setup => {
    exports => [qw(
        outer_stack_push
        outer_stack_pop
        outer_stack_peek
    )],
};

#pod =head1 DESCRIPTION
#pod
#pod This exporter collection contains the commonly used functions in
#pod L<MooseX::Declare>.
#pod
#pod All functions in this package will be exported upon request.
#pod
#pod =cut

my %OuterStack;


#pod =func outer_stack_push
#pod
#pod   outer_stack_push (Str $file, Str $value)
#pod
#pod Pushes the C<$value> on the internal stack for the file C<$file>.
#pod
#pod =cut

sub outer_stack_push {
    my ($file, $value) = @_;

    push @{ $OuterStack{ $file } }, $value;
    return $value;
}

#pod =func outer_stack_pop
#pod
#pod   outer_stack_pop (Str $file)
#pod
#pod Removes one item from the internal stack of the file C<$file>.
#pod
#pod =cut

sub outer_stack_pop {
    my ($file) = @_;

    return undef
        unless @{ $OuterStack{ $file } || [] };
    return pop @{ $OuterStack{ $file } };
}

#pod =func outer_stack_peek
#pod
#pod   outer_stack_peek (Str $file)
#pod
#pod Returns the topmost item in the internal stack for C<$file> without removing
#pod it from the stack.
#pod
#pod =cut

sub outer_stack_peek {
    my ($file) = @_;

    return undef
        unless @{ $OuterStack{ $file } || [] };
    return $OuterStack{ $file }[-1];
}

#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<MooseX::Declare>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare::Util - Common declarative utility functions

=head1 VERSION

version 0.43

=head1 DESCRIPTION

This exporter collection contains the commonly used functions in
L<MooseX::Declare>.

All functions in this package will be exported upon request.

=head1 FUNCTIONS

=head2 outer_stack_push

  outer_stack_push (Str $file, Str $value)

Pushes the C<$value> on the internal stack for the file C<$file>.

=head2 outer_stack_pop

  outer_stack_pop (Str $file)

Removes one item from the internal stack of the file C<$file>.

=head2 outer_stack_peek

  outer_stack_peek (Str $file)

Returns the topmost item in the internal stack for C<$file> without removing
it from the stack.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=back

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
