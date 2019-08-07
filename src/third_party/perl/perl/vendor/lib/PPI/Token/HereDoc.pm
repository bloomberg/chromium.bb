package PPI::Token::HereDoc;

=pod

=head1 NAME

PPI::Token::HereDoc - Token class for the here-doc

=head1 INHERITANCE

  PPI::Token::HereDoc
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

Here-docs are incredibly handy when writing Perl, but incredibly tricky
when parsing it, primarily because they don't follow the general flow of
input.

They jump ahead and nab lines directly off the input buffer. Whitespace
and newlines may not matter in most Perl code, but they matter in here-docs.

They are also tricky to store as an object. They look sort of like an
operator and a string, but they don't act like it. And they have a second
section that should be something like a separate token, but isn't because a
strong can span from above the here-doc content to below it.

So when parsing, this is what we do.

Firstly, the PPI::Token::HereDoc object, does not represent the C<<< << >>>
operator, or the "END_FLAG", or the content, or even the terminator.

It represents all of them at once.

The token itself has only the declaration part as its "content".

  # This is what the content of a HereDoc token is
  <<FOO
  
  # Or this
  <<"FOO"
  
  # Or even this
  <<      'FOO'

That is, the "operator", any whitespace separator, and the quoted or bare
terminator. So when you call the C<content> method on a HereDoc token, you
get '<< "FOO"'.

As for the content and the terminator, when treated purely in "content" terms
they do not exist.

The content is made available with the C<heredoc> method, and the name of
the terminator with the C<terminator> method.

To make things work in the way you expect, PPI has to play some games
when doing line/column location calculation for tokens, and also during
the content parsing and generation processes.

Documents cannot simply by recreated by stitching together the token
contents, and involve a somewhat more expensive procedure, but the extra
expense should be relatively negligible unless you are doing huge
quantities of them.

Please note that due to the immature nature of PPI in general, we expect
C<HereDocs> to be a rich (bad) source of corner-case bugs for quite a while,
but for the most part they should more or less DWYM.

=head2 Comparison to other string types

Although technically it can be considered a quote, for the time being
C<HereDocs> are being treated as a completely separate C<Token> subclass,
and will not be found in a search for L<PPI::Token::Quote> or
L<PPI::Token::QuoteLike objects>.

This may change in the future, with it most likely to end up under
QuoteLike.

=head1 METHODS

Although it has the standard set of C<Token> methods, C<HereDoc> objects
have a relatively large number of unique methods all of their own.

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}





#####################################################################
# PPI::Token::HereDoc Methods

=pod

=head2 heredoc

The C<heredoc> method is the authoritative method for accessing the contents
of the C<HereDoc> object.

It returns the contents of the here-doc as a list of newline-terminated
strings. If called in scalar context, it returns the number of lines in
the here-doc, B<excluding> the terminator line.

=cut

sub heredoc {
	wantarray
		? @{shift->{_heredoc}}
		: scalar @{shift->{_heredoc}};
}

=pod

=head2 terminator

The C<terminator> method returns the name of the terminating string for the
here-doc.

Returns the terminating string as an unescaped string (in the rare case
the terminator has an escaped quote in it).

=cut

sub terminator {
	shift->{_terminator};
}





#####################################################################
# Tokenizer Methods

# Parse in the entire here-doc in one call
sub __TOKENIZER__on_char {
	my $t     = $_[1];

	# We are currently located on the first char after the <<

	# Handle the most common form first for simplicity and speed reasons
	### FIXME - This regex, and this method in general, do not yet allow
	### for the null here-doc, which terminates at the first
	### empty line.
	my $rest_of_line = substr( $t->{line}, $t->{line_cursor} );
	unless ( $rest_of_line =~ /^( \s* (?: "[^"]*" | '[^']*' | `[^`]*` | \\?\w+ ) )/x  ) {
		# Degenerate to a left-shift operation
		$t->{token}->set_class('Operator');
		return $t->_finalize_token->__TOKENIZER__on_char( $t );
	}

	# Add the rest of the token, work out what type it is,
	# and suck in the content until the end.
	my $token = $t->{token};
	$token->{content} .= $1;
	$t->{line_cursor} += length $1;

	# Find the terminator, clean it up and determine
	# the type of here-doc we are dealing with.
	my $content = $token->{content};
	if ( $content =~ /^\<\<(\w+)$/ ) {
		# Bareword
		$token->{_mode}       = 'interpolate';
		$token->{_terminator} = $1;

	} elsif ( $content =~ /^\<\<\s*\'(.*)\'$/ ) {
		# ''-quoted literal
		$token->{_mode}       = 'literal';
		$token->{_terminator} = $1;
		$token->{_terminator} =~ s/\\'/'/g;

	} elsif ( $content =~ /^\<\<\s*\"(.*)\"$/ ) {
		# ""-quoted literal
		$token->{_mode}       = 'interpolate';
		$token->{_terminator} = $1;
		$token->{_terminator} =~ s/\\"/"/g;

	} elsif ( $content =~ /^\<\<\s*\`(.*)\`$/ ) {
		# ``-quoted command
		$token->{_mode}       = 'command';
		$token->{_terminator} = $1;
		$token->{_terminator} =~ s/\\`/`/g;

	} elsif ( $content =~ /^\<\<\\(\w+)$/ ) {
		# Legacy forward-slashed bareword
		$token->{_mode}       = 'literal';
		$token->{_terminator} = $1;

	} else {
		# WTF?
		return undef;
	}

	# Define $line outside of the loop, so that if we encounter the
	# end of the file, we have access to the last line still.
	my $line;

	# Suck in the HEREDOC
	$token->{_heredoc} = [];
	my $terminator = $token->{_terminator} . "\n";
	while ( defined($line = $t->_get_line) ) {
		if ( $line eq $terminator ) {
			# Keep the actual termination line for consistency
			# when we are re-assembling the file
			$token->{_terminator_line} = $line;

			# The HereDoc is now fully parsed
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		# Add the line
		push @{$token->{_heredoc}}, $line;
	}

	# End of file.
	# Error: Didn't reach end of here-doc before end of file.
	# $line might be undef if we get NO lines.
	if ( defined $line and $line eq $token->{_terminator} ) {
		# If the last line matches the terminator
		# but is missing the newline, we want to allow
		# it anyway (like perl itself does). In this case
		# perl would normally throw a warning, but we will
		# also ignore that as well.
		pop @{$token->{_heredoc}};
		$token->{_terminator_line} = $line;
	} else {
		# The HereDoc was not properly terminated.
		$token->{_terminator_line} = undef;

		# Trim off the trailing whitespace
		if ( defined $token->{_heredoc}->[-1] and $t->{source_eof_chop} ) {
			chop $token->{_heredoc}->[-1];
			$t->{source_eof_chop} = '';
		}
	}

	# Set a hint for PPI::Document->serialize so it can
	# inexpensively repair it if needed when writing back out.
	$token->{_damaged} = 1;

	# The HereDoc is not fully parsed
	$t->_finalize_token->__TOKENIZER__on_char( $t );
}

1;

=pod

=head1 TO DO

- Implement PPI::Token::Quote interface compatibility

- Check CPAN for any use of the null here-doc or here-doc-in-s///e

- Add support for the null here-doc

- Add support for here-doc in s///e

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
