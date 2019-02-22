package MooseX::Declare::Syntax::Keyword::Clean;
BEGIN {
  $MooseX::Declare::Syntax::Keyword::Clean::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::Keyword::Clean::VERSION = '0.35';
}
# ABSTRACT: Explicit namespace cleanups

use Moose;

use constant NAMESPACING_ROLE => 'MooseX::Declare::Syntax::NamespaceHandling';
use Carp qw( cluck );

use namespace::clean -except => 'meta';


with qw(
    MooseX::Declare::Syntax::KeywordHandling
);

sub find_namespace_handler {
    my ($self, $ctx) = @_;

    for my $item (reverse @{ $ctx->stack }) {
        return $item
            if $item->handler->does(NAMESPACING_ROLE);
    }

    return undef;
}


sub parse {
    my ($self, $ctx) = @_;

    if (my $stack_item = $self->find_namespace_handler($ctx)) {
        my $namespace = $stack_item->namespace;

        cluck "Attempted to clean an already cleaned namespace ($namespace). Did you mean to use 'is dirty'?"
            unless $stack_item->is_dirty;
    }

    $ctx->skip_declarator;
    $ctx->inject_code_parts_here(
        ';use namespace::clean -except => [qw( meta )]',
    );
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::Keyword::Clean - Explicit namespace cleanups

=head1 DESCRIPTION

This keyword will inject a call to L<namespace::clean> into its current
position.

=head1 METHODS

=head2 parse

  Object->parse(Object $context)

This will inject a call to L<namespace::clean> C<-except => 'meta'> into
the code at the position of the keyword.

=head1 CONSUMES

=over 4

=item *

L<MooseX::Declare::Syntax::KeywordHandling>

=back

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Syntax::KeywordHandling>

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

