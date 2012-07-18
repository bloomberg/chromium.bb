package MooseX::Declare::Syntax::Extending;
BEGIN {
  $MooseX::Declare::Syntax::Extending::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::Extending::VERSION = '0.35';
}
# ABSTRACT: Extending with superclasses

use Moose::Role;

use aliased 'MooseX::Declare::Context::Namespaced';

use namespace::clean -except => 'meta';


with qw(
    MooseX::Declare::Syntax::OptionHandling
);

around context_traits => sub { shift->(@_), Namespaced };


sub add_extends_option_customizations {
    my ($self, $ctx, $package, $superclasses) = @_;

    # add code for extends keyword
    $ctx->add_scope_code_parts(
        sprintf 'extends %s',
            join ', ',
            map  { "'$_'" }
            map  { $ctx->qualify_namespace($_) }
                @{ $superclasses },
    );

    return 1;
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::Extending - Extending with superclasses

=head1 DESCRIPTION

Extends a class by a specified C<extends> option.

=head1 METHODS

=head2 add_extends_option_customizations

  Object->add_extends_option_customizations (
      Object   $ctx,
      Str      $package,
      ArrayRef $superclasses,
      HashRef  $options
  )

This will add a code part that will call C<extends> with the C<$superclasses>
as arguments.

=head1 CONSUMES

=over 4

=item *

L<MooseX::Declare::Syntax::OptionHandling>

=back

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Syntax::Keyword::Class>

=item *

L<MooseX::Declare::Syntax::OptionHandling>

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

