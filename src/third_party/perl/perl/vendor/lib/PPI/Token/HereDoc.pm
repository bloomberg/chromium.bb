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
string can span from above the here-doc content to below it.

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
L<PPI::Token::QuoteLike> objects.

This may change in the future, with it most likely to end up under
QuoteLike.

=head1 METHODS

Although it has the standard set of C<Token> methods, C<HereDoc> objects
have a relatively large number of unique methods all of their own.

=cut

use strict;

our $VERSION = '1.269'; # VERSION

our @ISA = "PPI::Token";





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

sub heredoc { @{shift->{_heredoc}} }

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

sub _is_terminator {
	my ( $self, $terminator, $line, $indented ) = @_;
	if ( $indented ) {
		return $line =~ /^\s*\Q$terminator\E$/;
	} else {
		return $line eq $terminator;
	}
}

sub _indent {
	my ( $self, $token ) = @_;
	my ($indent) = $token->{_terminator_line} =~ /^(\s*)/;
	return $indent;
}

sub _is_match_indent {
	my ( $self, $token, $indent ) = @_;
	return (grep { /^$indent/ } @{$token->{_heredoc}}) == @{$token->{_heredoc}};
}




#####################################################################
# Tokenizer Methods

# Parse in the entire here-doc in one call
sub __TOKENIZER__on_char {
	my ( $self, $t ) = @_;

	# We are currently located on the first char after the <<

	# Handle the most common form first for simplicity and speed reasons
	### FIXME - This regex, and this method in general, do not yet allow
	### for the null here-doc, which terminates at the first
	### empty line.
	pos $t->{line} = $t->{line_cursor};

	if ( $t->{line} !~ m/\G( ~? \s* (?: "[^"]*" | '[^']*' | `[^`]*` | \\?\w+ ) )/gcx ) {
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
	if ( $content =~ /^\<\<(~?)(\w+)$/ ) {
		# Bareword
		$token->{_mode}       = 'interpolate';
		$token->{_indented}   = 1 if $1 eq '~';
		$token->{_terminator} = $2;

	} elsif ( $content =~ /^\<\<(~?)\s*\'(.*)\'$/ ) {
		# ''-quoted literal
		$token->{_mode}       = 'literal';
		$token->{_indented}   = 1 if $1 eq '~';
		$token->{_terminator} = $2;
		$token->{_terminator} =~ s/\\'/'/g;

	} elsif ( $content =~ /^\<\<(~?)\s*\"(.*)\"$/ ) {
		# ""-quoted literal
		$token->{_mode}       = 'interpolate';
		$token->{_indented}   = 1 if $1 eq '~';
		$token->{_terminator} = $2;
		$token->{_terminator} =~ s/\\"/"/g;

	} elsif ( $content =~ /^\<\<(~?)\s*\`(.*)\`$/ ) {
		# ``-quoted command
		$token->{_mode}       = 'command';
		$token->{_indented}   = 1 if $1 eq '~';
		$token->{_terminator} = $2;
		$token->{_terminator} =~ s/\\`/`/g;

	} elsif ( $content =~ /^\<\<(~?)\\(\w+)$/ ) {
		# Legacy forward-slashed bareword
		$token->{_mode}       = 'literal';
		$token->{_indented}   = 1 if $1 eq '~';
		$token->{_terminator} = $2;

	} else {
		# WTF?
		return undef;
	}

	# Suck in the HEREDOC
	$token->{_heredoc} = \my @heredoc;
	my $terminator = $token->{_terminator} . "\n";
	while ( defined( my $line = $t->_get_line ) ) {
		if ( $self->_is_terminator( $terminator, $line, $token->{_indented} ) ) {
			# Keep the actual termination line for consistency
			# when we are re-assembling the file
			$token->{_terminator_line} = $line;

			if ( $token->{_indented} ) {
				my $indent = $self->_indent( $token );
				# Indentation of here-doc doesn't match delimiter
				unless ( $self->_is_match_indent( $token, $indent ) ) {
					push @heredoc, $line;
					last;
				}

				s/^$indent// for @heredoc, $token->{_terminator_line};
			}

			# The HereDoc is now fully parsed
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		# Add the line
		push @heredoc, $line;
	}

	# End of file.
	# Error: Didn't reach end of here-doc before end of file.

	# If the here-doc block is not empty, look at the last line to determine if
	# the here-doc terminator is missing a newline (which Perl would fail to
	# compile but is easy to detect) or if the here-doc block was just not
	# terminated at all (which Perl would fail to compile as well).
	$token->{_terminator_line} = undef;
	if ( @heredoc and defined $heredoc[-1] ) {
		# See PPI::Tokenizer, the algorithm there adds a space at the end of the
		# document that we need to make sure we remove.
		if ( $t->{source_eof_chop} ) {
			chop $heredoc[-1];
			$t->{source_eof_chop} = '';
		}

		# Check if the last line of the file matches the terminator without
		# newline at the end. If so, remove it from the content and set it as
		# the terminator line.
		$token->{_terminator_line} = pop @heredoc
			if $self->_is_terminator( $token->{_terminator}, $heredoc[-1], $token->{_indented} );
	}

	if ( $token->{_indented} && $token->{_terminator_line} ) {
		my $indent = $self->_indent( $token );
		if ( $self->_is_match_indent( $token, $indent ) ) {
			# Remove indent from here-doc as much as possible
			s/^$indent// for @heredoc;
		}

		s/^$indent// for $token->{_terminator_line};
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
