package PPI::Token::Data;

=pod

=head1 NAME

PPI::Token::Data - The actual data in the __DATA__ section of a file

=head1 INHERITANCE

  PPI::Token::Data
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::Data> class is used to represent the actual data inside
a file's C<__DATA__> section.

One C<PPI::Token::Data> object is used to represent the entire of the data,
primarily so that it can provide a convenient handle directly to the data.

=head1 METHODS

C<PPI::Token::Data> provides one method in addition to those provided by
our parent L<PPI::Token> and L<PPI::Element> classes.

=cut

use strict;
use IO::String ();
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}





#####################################################################
# Methods

=pod

=head2 handle

The C<handle> method returns a L<IO::String> handle that allows you
to do all the normal handle-y things to the contents of the __DATA__
section of the file.

Unlike in perl itself, this means you can also do things like C<print>
new data onto the end of the __DATA__ section, or modify it with
any other process that can accept an L<IO::Handle> as input or output.

Returns an L<IO::String> object.

=cut

sub handle {
	my $self = shift;
	IO::String->new( \$self->{content} );
}

sub __TOKENIZER__on_char { 1 }

1;

=pod

=head1 SUPPORT

See the L<support section|PPI/SUPPORT> in the main module.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright 2001 - 2011 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
