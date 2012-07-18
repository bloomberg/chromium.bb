package MooseX::Declare::Syntax::RoleApplication;
BEGIN {
  $MooseX::Declare::Syntax::RoleApplication::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::RoleApplication::VERSION = '0.35';
}
# ABSTRACT: Handle user specified roles

use Moose::Role;

use aliased 'MooseX::Declare::Context::Namespaced';

use namespace::clean -except => 'meta';


with qw(
    MooseX::Declare::Syntax::OptionHandling
);

around context_traits => sub { shift->(@_), Namespaced };


sub add_with_option_customizations {
    my ($self, $ctx, $package, $roles) = @_;

    # consume roles
    $ctx->add_early_cleanup_code_parts(
        sprintf 'Moose::Util::apply_all_roles(%s->meta, %s)',
            $package,
            join ', ',
            map  { "q[$_]" }
            map  { $ctx->qualify_namespace($_) }
                @{ $roles },
    );

    return 1;
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::RoleApplication - Handle user specified roles

=head1 DESCRIPTION

This role extends L<MooseX::Declare::Syntax::OptionHandling> and provides
a C<with|/add_with_option_customizations> option.

=head1 METHODS

=head2 add_with_option_customizations

  Object->add_with_option_customizations (
      Object   $context,
      Str      $package,
      ArrayRef $roles,
      HashRef  $options
  )

This will add a call to C<with> in the scope code.

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

