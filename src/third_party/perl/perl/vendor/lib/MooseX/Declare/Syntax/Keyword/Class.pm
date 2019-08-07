package MooseX::Declare::Syntax::Keyword::Class;
BEGIN {
  $MooseX::Declare::Syntax::Keyword::Class::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::Keyword::Class::VERSION = '0.35';
}
# ABSTRACT: Class declarations

use Moose;

use namespace::clean -except => 'meta';


with qw(
    MooseX::Declare::Syntax::MooseSetup
    MooseX::Declare::Syntax::RoleApplication
    MooseX::Declare::Syntax::Extending
);


around imported_moose_symbols => sub { shift->(@_), qw( extends has inner super ) };


sub generate_export { my $self = shift; sub { $self->make_anon_metaclass } }



around auto_make_immutable => sub { 1 };


around make_anon_metaclass => sub { Moose::Meta::Class->create_anon_class };


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::Keyword::Class - Class declarations

=head1 METHODS

=head2 generate_export

  CodeRef generate_export ()

This will return a closure doing a call to L</make_anon_metaclass>.

=head1 CONSUMES

=over 4

=item *

L<MooseX::Declare::Syntax::MooseSetup>

=item *

L<MooseX::Declare::Syntax::RoleApplication>

=item *

L<MooseX::Declare::Syntax::Extending>

=back

=head1 MODIFIED METHODS

=head2 imported_moose_symbols

  List Object->imported_moose_symbols ()

Extends the existing L<MooseX::Declare::Syntax::MooseSetup/imported_moose_symbols>
with C<extends>, C<has>, C<inner> and C<super>.

=head2 auto_make_immutable

  Bool Object->auto_make_immutable ()

Is set to a true value, so classes are made immutable by default.

=head2 make_anon_metaclass

  Object Object->make_anon_metaclass ()

Returns an anonymous instance of L<Moose::Meta::Class>.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Syntax::Keyword::Role>

=item *

L<MooseX::Declare::Syntax::RoleApplication>

=item *

L<MooseX::Declare::Syntax::Extending>

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

