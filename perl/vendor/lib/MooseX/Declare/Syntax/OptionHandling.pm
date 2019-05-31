package MooseX::Declare::Syntax::OptionHandling;
# ABSTRACT: Option parser dispatching

our $VERSION = '0.43';

use Moose::Role;
use Carp qw( croak );
use namespace::autoclean;

#pod =head1 DESCRIPTION
#pod
#pod This role will call a C<add_foo_option_customization> for every C<foo> option
#pod that is discovered.
#pod
#pod =head1 REQUIRED METHODS
#pod
#pod =head2 get_identifier
#pod
#pod   Str Object->get_identifier ()
#pod
#pod This must return the name of the current keyword's identifier.
#pod
#pod =cut

requires qw( get_identifier );

#pod =method ignored_options
#pod
#pod   List[Str] Object->ignored_options ()
#pod
#pod This method returns a list of option names that won't be dispatched. By default
#pod this only contains the C<is> option.
#pod
#pod =cut

sub ignored_options { qw( is ) }


#pod =head1 MODIFIED METHODS
#pod
#pod =head2 add_optional_customizations
#pod
#pod   Object->add_optional_customizations (Object $context, Str $package, HashRef $options)
#pod
#pod This will dispatch to the respective C<add_*_option_customization> method for option
#pod handling unless the option is listed in the L</ignored_options>.
#pod
#pod =cut

after add_optional_customizations => sub {
    my ($self, $ctx, $package) = @_;
    my $options = $ctx->options;

    # ignored options
    my %ignored = map { ($_ => 1) } $self->ignored_options;

    # try to find a handler for each option
    for my $option (keys %$options) {
        next if $ignored{ $option };

        # call the handler with its own value and all options
        if (my $method = $self->can("add_${option}_option_customizations")) {
            $self->$method($ctx, $package, $options->{ $option }, $options);
        }

        # no handler method was found
        else {
            croak sprintf q/The '%s' keyword does not know what to do with an '%s' option/,
                $self->get_identifier,
                $option;
        }
    }

    return 1;
};

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

MooseX::Declare::Syntax::OptionHandling - Option parser dispatching

=head1 VERSION

version 0.43

=head1 DESCRIPTION

This role will call a C<add_foo_option_customization> for every C<foo> option
that is discovered.

=head1 METHODS

=head2 ignored_options

  List[Str] Object->ignored_options ()

This method returns a list of option names that won't be dispatched. By default
this only contains the C<is> option.

=head1 REQUIRED METHODS

=head2 get_identifier

  Str Object->get_identifier ()

This must return the name of the current keyword's identifier.

=head1 MODIFIED METHODS

=head2 add_optional_customizations

  Object->add_optional_customizations (Object $context, Str $package, HashRef $options)

This will dispatch to the respective C<add_*_option_customization> method for option
handling unless the option is listed in the L</ignored_options>.

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
