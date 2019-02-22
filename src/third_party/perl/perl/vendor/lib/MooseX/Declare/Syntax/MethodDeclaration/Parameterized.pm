package MooseX::Declare::Syntax::MethodDeclaration::Parameterized;
BEGIN {
  $MooseX::Declare::Syntax::MethodDeclaration::Parameterized::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::MethodDeclaration::Parameterized::VERSION = '0.35';
}

use Moose::Role;
use MooseX::Role::Parameterized 0.12 ();
use namespace::autoclean;

around register_method_declaration => sub {
    my ($next, $self, $parameterizable_meta, $name, $method) = @_;
    my $meta = $self->metaclass_for_method_application($parameterizable_meta, $name, $method);
    $self->$next($meta, $name, $method);
};

sub metaclass_for_method_application {
    return MooseX::Role::Parameterized->current_metaclass;
}

1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::MethodDeclaration::Parameterized

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

