package MooseX::Declare::Context::Namespaced;
BEGIN {
  $MooseX::Declare::Context::Namespaced::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Context::Namespaced::VERSION = '0.35';
}
# ABSTRACT: Namespaced context

use Moose::Role;

use Carp                  qw( croak );
use MooseX::Declare::Util qw( outer_stack_peek );

use namespace::clean -except => 'meta';


has namespace => (
    is          => 'rw',
    isa         => 'Str',
);



sub strip_namespace {
    my ($self) = @_;

    my $namespace = $self->strip_word;

    $self->namespace($namespace)
        if defined $namespace and length $namespace;

    return $namespace;
}


sub qualify_namespace {
    my ($self, $namespace) = @_;

    # only qualify namespaces starting with ::
    return $namespace
        unless $namespace =~ /^::/;

    # try to find the enclosing package
    my $outer = outer_stack_peek($self->caller_file)
        or croak "No outer namespace found to apply relative $namespace to";

    return $outer . $namespace;
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Context::Namespaced - Namespaced context

=head1 DESCRIPTION

This context trait will add namespace functionality to the context.

=head1 ATTRIBUTES

=head2 namespace

This will be set when the C<strip_namespace> method is called and the
namespace wasn't anonymous. It will contain the specified namespace, not
the fully qualified one.

=head1 METHODS

=head2 strip_namespace

  Maybe[Str] Object->strip_namespace()

This method is intended to parse the main namespace of a namespaced keyword.
It will use L<Devel::Declare::Context::Simple>s C<strip_word> method and store
the result in the L</namespace> attribute if true.

=head2 qualify_namespace

  Str Object->qualify_namespace(Str $namespace)

If the C<$namespace> passed it begins with a C<::>, it will be prefixed with
the outer namespace in the file. If there is no outer namespace, an error
will be thrown.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Context>

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

