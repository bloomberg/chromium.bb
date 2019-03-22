package MooseX::Declare::StackItem;
BEGIN {
  $MooseX::Declare::StackItem::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::StackItem::VERSION = '0.35';
}

use Moose;

use namespace::clean -except => 'meta';
use overload '""' => 'as_string', fallback => 1;

has identifier => (
    is          => 'rw',
    isa         => 'Str',
    required    => 1,
);

has handler => (
    is          => 'ro',
    required    => 1,
    default     => '',
);

has is_dirty => (
    is          => 'ro',
    isa         => 'Bool',
);

has is_parameterized => (
    is  => 'ro',
    isa => 'Bool',
);

has namespace => (
    is          => 'ro',
    isa         => 'Str|Undef',

);

sub as_string {
    my ($self) = @_;
    return $self->identifier;
}

sub serialize {
    my ($self) = @_;
    return sprintf '%s->new(%s)',
        ref($self),
        join ', ', map { defined($_) ? "q($_)" : 'undef' }
        'identifier',       $self->identifier,
        'handler',          $self->handler,
        'is_dirty',         ( $self->is_dirty         ? 1 : 0 ),
        'is_parameterized', ( $self->is_parameterized ? 1 : 0 ),
        'namespace',        $self->namespace,
        ;
}

__PACKAGE__->meta->make_immutable;

1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::StackItem

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

