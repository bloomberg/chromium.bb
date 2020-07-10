package MooseX::Declare::Syntax::Keyword::Class;
# ABSTRACT: Class declarations

our $VERSION = '0.43';

use Moose;
use namespace::autoclean;

#pod =head1 CONSUMES
#pod
#pod =for :list
#pod * L<MooseX::Declare::Syntax::MooseSetup>
#pod * L<MooseX::Declare::Syntax::RoleApplication>
#pod * L<MooseX::Declare::Syntax::Extending>
#pod
#pod =cut

with qw(
    MooseX::Declare::Syntax::MooseSetup
    MooseX::Declare::Syntax::RoleApplication
    MooseX::Declare::Syntax::Extending
);

#pod =head1 MODIFIED METHODS
#pod
#pod =head2 imported_moose_symbols
#pod
#pod   List Object->imported_moose_symbols ()
#pod
#pod Extends the existing L<MooseX::Declare::Syntax::MooseSetup/imported_moose_symbols>
#pod with C<extends>, C<has>, C<inner> and C<super>.
#pod
#pod =cut

around imported_moose_symbols => sub { shift->(@_), qw( extends has inner super ) };

#pod =method generate_export
#pod
#pod   CodeRef generate_export ()
#pod
#pod This will return a closure doing a call to L</make_anon_metaclass>.
#pod
#pod =cut

sub generate_export { my $self = shift; sub { $self->make_anon_metaclass } }


#pod =head2 auto_make_immutable
#pod
#pod   Bool Object->auto_make_immutable ()
#pod
#pod Is set to a true value, so classes are made immutable by default.
#pod
#pod =cut

around auto_make_immutable => sub { 1 };

#pod =head2 make_anon_metaclass
#pod
#pod   Object Object->make_anon_metaclass ()
#pod
#pod Returns an anonymous instance of L<Moose::Meta::Class>.
#pod
#pod =cut

around make_anon_metaclass => sub { Moose::Meta::Class->create_anon_class };

#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<MooseX::Declare>
#pod * L<MooseX::Declare::Syntax::Keyword::Role>
#pod * L<MooseX::Declare::Syntax::RoleApplication>
#pod * L<MooseX::Declare::Syntax::Extending>
#pod * L<MooseX::Declare::Syntax::MooseSetup>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare::Syntax::Keyword::Class - Class declarations

=head1 VERSION

version 0.43

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

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
