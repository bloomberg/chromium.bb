package PPI::Token::Comment;

=pod

=head1 NAME

PPI::Token::Comment - A comment in Perl source code

=head1 INHERITANCE

  PPI::Token::Comment
  isa PPI::Token
      isa PPI::Element

=head1 SYNOPSIS

  # This is a PPI::Token::Comment
  
  print "Hello World!"; # So it this
  
  $string =~ s/ foo  # This, unfortunately, is not :(
        bar
  	/w;

=head1 DESCRIPTION

In PPI, comments are represented by C<PPI::Token::Comment> objects.

These come in two flavours, line comment and inline comments.

A C<line comment> is a comment that stands on its own line. These comments
hold their own newline and whitespace (both leading and trailing) as part
of the one C<PPI::Token::Comment> object.

An inline comment is a comment that appears after some code, and
continues to the end of the line. This does B<not> include whitespace,
and the terminating newlines is considered a separate
L<PPI::Token::Whitespace> token.

This is largely a convenience, simplifying a lot of normal code relating
to the common things people do with comments.

Most commonly, it means when you C<prune> or C<delete> a comment, a line
comment disappears taking the entire line with it, and an inline comment
is removed from the inside of the line, allowing the newline to drop
back onto the end of the code, as you would expect.

It also means you can move comments around in blocks much more easily.

For now, this is a suitably handy way to do things. However, I do reserve
the right to change my mind on this one if it gets dangerously
anachronistic somewhere down the line.

=head1 METHODS

Only very limited methods are available, beyond those provided by our
parent L<PPI::Token> and L<PPI::Element> classes.

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}

### XS -> PPI/XS.xs:_PPI_Token_Comment__significant 0.900+
sub significant { '' }

# Most stuff goes through __TOKENIZER__commit.
# This is such a rare case, do char at a time to keep the code small
sub __TOKENIZER__on_char {
	my $t = $_[1];

	# Make sure not to include the trailing newline
	if ( substr( $t->{line}, $t->{line_cursor}, 1 ) eq "\n" ) {
		return $t->_finalize_token->__TOKENIZER__on_char( $t );
	}

	1;
}

sub __TOKENIZER__commit {
	my $t = $_[1];

	# Get the rest of the line
	my $rest = substr( $t->{line}, $t->{line_cursor} );
	if ( chomp $rest ) { # Include the newline separately
		# Add the current token, and the newline
		$t->_new_token('Comment', $rest);
		$t->_new_token('Whitespace', "\n");
	} else {
		# Add this token only
		$t->_new_token('Comment', $rest);
	}

	# Advance the line cursor to the end
	$t->{line_cursor} = $t->{line_length} - 1;

	0;
}

# Comments end at the end of the line
sub __TOKENIZER__on_line_end {
	$_[1]->_finalize_token if $_[1]->{token};
	1;
}

=pod

=head2 line

The C<line> accessor returns true if the C<PPI::Token::Comment> is a
line comment, or false if it is an inline comment.

=cut

sub line {
	# Entire line comments have a newline at the end
	$_[0]->{content} =~ /\n$/ ? 1 : 0;
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
