package MooseX::Declare::Syntax::Extending;
# ABSTRACT: Extending with superclasses

our $VERSION = '0.43';

use Moose::Role;
use aliased 'MooseX::Declare::Context::Namespaced';
use namespace::autoclean;

#pod =head1 DESCRIPTION
#pod
#pod Extends a class by a specified C<extends> option.
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

#pod =method add_extends_option_customizations
#pod
#pod   Object->add_extends_option_customizations (
#pod       Object   $ctx,
#pod       Str      $package,
#pod       ArrayRef $superclasses,
#pod       HashRef  $options
#pod   )
#pod
#pod This will add a code part that will call C<extends> with the C<$superclasses>
#pod as arguments.
#pod
#pod =cut

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

#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<MooseX::Declare>
#pod * L<MooseX::Declare::Syntax::Keyword::Class>
#pod * L<MooseX::Declare::Syntax::OptionHandling>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare::Syntax::Extending - Extending with superclasses

=head1 VERSION

version 0.43

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

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
