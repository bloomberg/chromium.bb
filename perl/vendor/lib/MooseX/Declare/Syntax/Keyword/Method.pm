package MooseX::Declare::Syntax::Keyword::Method;
BEGIN {
  $MooseX::Declare::Syntax::Keyword::Method::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::Keyword::Method::VERSION = '0.35';
}
# ABSTRACT: Handle method declarations

use Moose;

use namespace::clean -except => 'meta';


with 'MooseX::Declare::Syntax::MethodDeclaration';


sub register_method_declaration {
    my ($self, $meta, $name, $method) = @_;
    return $meta->add_method($name, $method);
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::Keyword::Method - Handle method declarations

=head1 DESCRIPTION

This role is an extension of L<MooseX::Declare::Syntax::MethodDeclaration>
that allows you to install keywords that declare methods.

=head1 METHODS

=head2 register_method_declaration

  Object->register_method_declaration (Object $metaclass, Str $name, Object $method)

This method required by the method declaration role will register the finished
method object via the C<< $metaclass->add_method >> method.

  MethodModifier->new(
      identifier           => 'around',
      modifier_type        => 'around',
      prototype_injections => {
          declarator => 'around',
          injections => [ 'CodeRef $orig' ],
      },
  );

This will mean that the signature C<(Str $foo)> will become
C<CodeRef $orig: Object $self, Str $foo> and and C<()> will become
C<CodeRef $orig: Object $self>.

=head1 CONSUMES

=over 4

=item *

L<MooseX::Declare::Syntax::MethodDeclaration>

=back

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Syntax::MooseSetup>

=item *

L<MooseX::Declare::Syntax::MethodDeclaration>

=item *

L<MooseX::Method::Signatures>

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

