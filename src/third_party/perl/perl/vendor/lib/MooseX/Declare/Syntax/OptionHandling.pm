package MooseX::Declare::Syntax::OptionHandling;
BEGIN {
  $MooseX::Declare::Syntax::OptionHandling::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::OptionHandling::VERSION = '0.35';
}
# ABSTRACT: Option parser dispatching

use Moose::Role;

use Carp qw( croak );

use namespace::clean -except => 'meta';


requires qw( get_identifier );


sub ignored_options { qw( is ) }



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


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::OptionHandling - Option parser dispatching

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

