package PPI::Token::Prototype;

=pod

=head1 NAME

PPI::Token::Prototype - A subroutine prototype descriptor

=head1 INHERITANCE

  PPI::Token::End
  isa PPI::Token
      isa PPI::Element

=head1 SYNOPSIS

  sub ($@) prototype;

=head1 DESCRIPTION

Although it sort of looks like a list or condition, a subroutine
prototype is a lot more like a string. Its job is to provide hints
to the perl compiler on what type of arguments a particular subroutine
expects, which the compiler uses to validate parameters at compile-time,
and allows programmers to use the functions without explicit parameter
parens.

Due to the rise of OO Perl coding, which ignores these prototypes, they
are most often used to allow for constant-like things, and to "extend"
the language and create things that act like keywords and core functions.

  # Create something that acts like a constant
  sub MYCONSTANT () { 10 }
  
  # Create the "any" core-looking function
  sub any (&@) { ... }
  
  if ( any { $_->cute } @babies ) {
  	...
  }

=head1 METHODS

This class provides one additional method beyond those defined by the
L<PPI::Token> and L<PPI::Element> parent classes.

=cut

use strict;
use PPI::Token ();

our $VERSION = '1.269'; # VERSION

our @ISA = "PPI::Token";

sub __TOKENIZER__on_char {
	my $class = shift;
	my $t     = shift;

	# Suck in until we find the closing paren (or the end of line)
	pos $t->{line} = $t->{line_cursor};
	die "regex should always match" if $t->{line} !~ m/\G(.*?(?:\)|$))/gc;
	$t->{token}->{content} .= $1;
	$t->{line_cursor} += length $1;

	# Shortcut if end of line
	return 0 unless $1 =~ /\)$/;

	# Found the closing paren
	$t->_finalize_token->__TOKENIZER__on_char( $t );
}

=pod

=head2 prototype

The C<prototype> accessor returns the actual prototype pattern, stripped
of flanking parens and of all whitespace. This mirrors the behavior of
the Perl C<prototype> builtin function.

Note that stripping parens and whitespace means that the return of
C<prototype> can be an empty string.

=cut

sub prototype {
	my $self  = shift;
	my $proto = $self->content;
	$proto =~ s/(^\(|\)$|\s+)//g;
	$proto;
}

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
