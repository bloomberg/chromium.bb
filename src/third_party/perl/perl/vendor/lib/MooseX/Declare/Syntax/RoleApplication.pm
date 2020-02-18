package MooseX::Declare::Syntax::RoleApplication;
# ABSTRACT: Handle user specified roles

our $VERSION = '0.43';

use Moose::Role;
use aliased 'MooseX::Declare::Context::Namespaced';
use namespace::autoclean;

#pod =head1 DESCRIPTION
#pod
#pod This role extends L<MooseX::Declare::Syntax::OptionHandling> and provides
#pod a C<with|/add_with_option_customizations> option.
#pod
#pod =head1 CONSUMES
#pod
#pod =for :list
#pod * L<MooseX::Declare::Syntax::OptionHandling>
#pod
#pod =cut

with qw(
    MooseX::Declare::Syntax::OptionHandling
);

around context_traits => sub { shift->(@_), Namespaced };

#pod =method add_with_option_customizations
#pod
#pod   Object->add_with_option_customizations (
#pod       Object   $context,
#pod       Str      $package,
#pod       ArrayRef $roles,
#pod       HashRef  $options
#pod   )
#pod
#pod This will add a call to C<with> in the scope code.
#pod
#pod =cut

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

#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<MooseX::Declare>
#pod * L<MooseX::Declare::Syntax::OptionHandling>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare::Syntax::RoleApplication - Handle user specified roles

=head1 VERSION

version 0.43

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

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
