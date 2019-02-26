package MooseX::Declare::Syntax::EmptyBlockIfMissing;
BEGIN {
  $MooseX::Declare::Syntax::EmptyBlockIfMissing::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::EmptyBlockIfMissing::VERSION = '0.35';
}
# ABSTRACT: Handle missing blocks after keywords

use Moose::Role;

use namespace::clean -except => 'meta';


sub handle_missing_block {
    my ($self, $ctx, $inject, %args) = @_;

    # default to block with nothing more than the default contents
    $ctx->inject_code_parts_here("{ $inject }");
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::EmptyBlockIfMissing - Handle missing blocks after keywords

=head1 DESCRIPTION

The L<MooseX::Declare::Syntax::NamespaceHandling> role will require that the
consumer handles the case of non-existant blocks. This role will inject
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

