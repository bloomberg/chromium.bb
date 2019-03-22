package PPI::Token::_QuoteEngine;

=pod

=head1 NAME

PPI::Token::_QuoteEngine - The PPI Quote Engine

=head1 DESCRIPTION

The C<PPI::Token::_QuoteEngine> package is designed hold functionality
for processing quotes and quote like operators, including regexes.
These have special requirements in parsing.

The C<PPI::Token::_QuoteEngine> package itself provides various parsing
methods, which the L<PPI::Token::Quote>, L<PPI::Token::QuoteLike> and
L<PPI::Token::Regexp> can inherit from. In this sense, it serves
as a base class.

=head2 Using this class

I<(Refers only to internal uses. This class does not provide a
public interface)>

To use these, you should initialize them as normal C<'$Class-E<gt>new'>,
and then call the 'fill' method, which will cause the specialised
parser to scan forwards and parse the quote to its end point.

If -E<gt>fill returns true, finalise the token.

=cut

use strict;
use Carp ();

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.215';
}





# Hook for the __TOKENIZER__on_char token call
sub __TOKENIZER__on_char {
	my $class = shift;
	my $t     = $_[0]->{token} ? shift : return undef;

	# Call the fill method to process the quote
	my $rv = $t->{token}->_fill( $t );
	return undef unless defined $rv;

	## Doesn't support "end of file" indicator

	# Finalize the token and return 0 to tell the tokenizer
	# to go to the next character.
	$t->_finalize_token;

	0;
}





#####################################################################
# Optimised character processors, used for quotes
# and quote like stuff, and accessible to the child classes

# An outright scan, raw and fast.
# Searches for a particular character, loading in new
# lines as needed.
# When called, we start at the current position.
# When leaving, the position should be set to the position
# of the character, NOT the one after it.
sub _scan_for_character {
	my $class = shift;
	my $t     = shift;
	my $char  = (length $_[0] == 1) ? quotemeta shift : return undef;

	# Create the search regex
	my $search = qr/^(.*?$char)/;

	my $string = '';
	while ( exists $t->{line} ) {
		# Get the search area for the current line
		my $search_area
			= $t->{line_cursor}
			? substr( $t->{line}, $t->{line_cursor} )
			: $t->{line};

		# Can we find a match on this line
		if ( $search_area =~ /$search/ ) {
			# Found the character on this line
			$t->{line_cursor} += length($1) - 1;
			return $string . $1;
		}

		# Load in the next line
		$string .= $search_area;
		return undef unless defined $t->_fill_line;
		$t->{line_cursor} = 0;
	}

	# Returning the string as a reference indicates EOF
	\$string;
}

# Scan for a character, but not if it is escaped
sub _scan_for_unescaped_character {
	my $class = shift;
	my $t     = shift;
	my $char  = (length $_[0] == 1) ? quotemeta shift : return undef;

	# Create the search regex.
	# Same as above but with a negative look-behind assertion.
	my $search = qr/^(.*?(?<!\\)(?:\\\\)*$char)/;

	my $string = '';
	while ( exists $t->{line} ) {
		# Get the search area for the current line
		my $search_area
			= $t->{line_cursor}
			? substr( $t->{line}, $t->{line_cursor} )
			: $t->{line};

		# Can we find a match on this line
		if ( $search_area =~ /$search/ ) {
			# Found the character on this line
			$t->{line_cursor} += length($1) - 1;
			return $string . $1;
		}

		# Load in the next line
		$string .= $search_area;
		my $rv = $t->_fill_line('inscan');
		if ( $rv ) {
			# Push to first character
			$t->{line_cursor} = 0;
		} elsif ( defined $rv ) {
			# We hit the End of File
			return \$string;
		} else {
			# Unexpected error
			return undef;
		}
	}

	# We shouldn't be able to get here
	return undef;
}

# Scan for a close braced, and take into account both escaping,
# and open close bracket pairs in the string. When complete, the
# method leaves the line cursor on the LAST character found.
sub _scan_for_brace_character {
	my $class       = shift;
	my $t           = shift;
	my $close_brace = $_[0] =~ /^(?:\>|\)|\}|\])$/ ? shift : Carp::confess(''); # return undef;
	my $open_brace  = $close_brace;
	$open_brace =~ tr/\>\)\}\]/\<\(\{\[/;

	# Create the search string
	$close_brace = quotemeta $close_brace;
	$open_brace = quotemeta $open_brace;
	my $search = qr/^(.*?(?<!\\)(?:\\\\)*(?:$open_brace|$close_brace))/;

	# Loop as long as we can get new lines
	my $string = '';
	my $depth = 1;
	while ( exists $t->{line} ) {
		# Get the search area
		my $search_area
			= $t->{line_cursor}
			? substr( $t->{line}, $t->{line_cursor} )
			: $t->{line};

		# Look for a match
		unless ( $search_area =~ /$search/ ) {
			# Load in the next line
			$string .= $search_area;
			my $rv = $t->_fill_line('inscan');
			if ( $rv ) {
				# Push to first character
				$t->{line_cursor} = 0;
				next;
			}
			if ( defined $rv ) {
				# We hit the End of File
				return \$string;
			}

			# Unexpected error
			return undef;
		}

		# Add to the string
		$string .= $1;
		$t->{line_cursor} += length $1;

		# Alter the depth and continue if we arn't at the end
		$depth += ($1 =~ /$open_brace$/) ? 1 : -1 and next;

		# Rewind the cursor by one character ( cludgy hack )
		$t->{line_cursor} -= 1;
		return $string;
	}

	# Returning the string as a reference indicates EOF
	\$string;
}

# Find all spaces and comments, up to, but not including
# the first non-whitespace character.
#
# Although it doesn't return it, it leaves the cursor
# on the character following the gap
sub _scan_quote_like_operator_gap {
	my $t = $_[1];

	my $string = '';
	while ( exists $t->{line} ) {
		# Get the search area for the current line
		my $search_area
			= $t->{line_cursor}
			? substr( $t->{line}, $t->{line_cursor} )
			: $t->{line};

		# Since this regex can match zero characters, it should always match
		$search_area =~ /^(\s*(?:\#.*)?)/s or return undef;

		# Add the chars found to the string
		$string .= $1;

		# Did we match the entire line?
		unless ( length $1 == length $search_area ) {
			# Partial line match, which means we are at
			# the end of the gap. Fix the cursor and return
			# the string.
			$t->{line_cursor} += length $1;
			return $string;
		}

		# Load in the next line.
		# If we reach the EOF, $t->{line} gets deleted,
		# which is caught by the while.
		my $rv = $t->_fill_line('inscan');
		if ( $rv ) {
			# Set the cursor to the first character
			$t->{line_cursor} = 0;
		} elsif ( defined $rv ) {
			# Returning the string as a reference indicates EOF
			return \$string;
		} else {
			return undef;
		}
	}

	# Shouldn't be able to get here
	return undef;
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
