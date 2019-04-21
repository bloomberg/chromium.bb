use strict;
use warnings;

package MooseX::Declare::Util;
BEGIN {
  $MooseX::Declare::Util::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Util::VERSION = '0.35';
}
# ABSTRACT: Common declarative utility functions

use Sub::Exporter -setup => {
    exports => [qw(
        outer_stack_push
        outer_stack_pop
        outer_stack_peek
    )],
};


my %OuterStack;



sub outer_stack_push {
    my ($file, $value) = @_;

    push @{ $OuterStack{ $file } }, $value;
    return $value;
}


sub outer_stack_pop {
    my ($file) = @_;

    return undef
        unless @{ $OuterStack{ $file } || [] };
    return pop @{ $OuterStack{ $file } };
}


sub outer_stack_peek {
    my ($file) = @_;

    return undef
        unless @{ $OuterStack{ $file } || [] };
    return $OuterStack{ $file }[-1];
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Util - Common declarative utility functions

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

=head1 AUTHORS

=over 4

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Ash Berlin <ash@cpan.org>

=item *

Chas. J. Owens IV <chas.owens@gmail.com>

=item *

Chris Prather <chris@prather.org>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Devin Austin <dhoss@cpan.org>

=item *

Hans Dieter Pearcey <hdp@cpan.org>

=item *

Justin Hunter <justin.d.hunter@gmail.com>

=item *

Matt Kraai <kraai@ftbfs.org>

=item *

Michele Beltrame <arthas@cpan.org>

=item *

Nelo Onyiah <nelo.onyiah@gmail.com>

=item *

nperez <nperez@cpan.org>

=item *

Piers Cawley <pdcawley@bofh.org.uk>

=item *

Rafael Kitover <rkitover@io.com>

=item *

Robert 'phaylon' Sedlacek <rs@474.at>

=item *

Stevan Little <stevan.little@iinteractive.com>

=item *

Tomas Doran <bobtfish@bobtfish.net>

=item *

Yanick Champoux <yanick@babyl.dyndns.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

