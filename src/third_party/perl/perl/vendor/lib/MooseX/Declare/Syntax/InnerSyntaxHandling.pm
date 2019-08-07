package MooseX::Declare::Syntax::InnerSyntaxHandling;
BEGIN {
  $MooseX::Declare::Syntax::InnerSyntaxHandling::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::InnerSyntaxHandling::VERSION = '0.35';
}
# ABSTRACT: Keywords inside blocks

use Moose::Role;

use MooseX::Declare::Util qw( outer_stack_push );

use namespace::clean -except => 'meta';


requires qw(
    get_identifier
);


sub default_inner { [] }


after setup_for => sub {
    my ($self, $setup_class, %args) = @_;

    # make sure stack is valid
    my $stack = $args{stack} || [];

    # setup inner keywords if we're inside ourself
    if (grep { $_ eq $self->get_identifier } @$stack) {
        $self->setup_inner_for($setup_class, %args);
    }
};


sub setup_inner_for {
    my ($self, $setup_class, %args) = @_;

    # setup each keyword in target class
    for my $inner (@{ $self->default_inner($args{stack}) }) {
        $inner->setup_for($setup_class, %args);
    }

    # push package onto stack for namespace management
    if (exists $args{file}) {
        outer_stack_push $args{file}, $args{outer_package};
    }
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::InnerSyntaxHandling - Keywords inside blocks

=head1 DESCRIPTION

This role allows you to setup keyword handlers that are only available
inside blocks or other scoping environments.

=head1 METHODS

=head2 default_inner

  ArrayRef[Object] Object->default_inner ()

Returns an empty C<ArrayRef> by default. If you want to setup additional
keywords you will have to C<around> this method.

=head2 setup_inner_for

  Object->setup_inner_for(ClassName $class, %args)

Sets up all handlers in the inner class.

=head1 REQUIRED METHODS

=head2 get_identifier

  Str get_identifier ()

Required to return the name of the identifier of the current handler.

=head1 MODIFIED METHODS

=head2 setup_for

  Object->setup_for(ClassName $class, %args)

After the keyword is setup inside itself, this will call L</setup_inner_for>.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Syntax::NamespaceHandling>

=item *

L<MooseX::Declare::Syntax::MooseSetup>

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

