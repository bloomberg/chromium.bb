package PPI::Token::_QuoteEngine::Full;

# Full quote engine

use strict;
use Clone                    ();
use Carp                     ();
use PPI::Token::_QuoteEngine ();

our $VERSION = '1.269'; # VERSION

our @ISA = 'PPI::Token::_QuoteEngine';

# Prototypes for the different braced sections
my %SECTIONS = (
	'(' => { type => '()', _close => ')' },
	'<' => { type => '<>', _close => '>' },
	'[' => { type => '[]', _close => ']' },
	'{' => { type => '{}', _close => '}' },
);

# For each quote type, the extra fields that should be set.
# This should give us faster initialization.
my %QUOTES = (
	'q'   => { operator => 'q',   braced => undef, separator => undef, _sections => 1 },
	'qq'  => { operator => 'qq',  braced => undef, separator => undef, _sections => 1 },
	'qx'  => { operator => 'qx',  braced => undef, separator => undef, _sections => 1 },
	'qw'  => { operator => 'qw',  braced => undef, separator => undef, _sections => 1 },
	'qr'  => { operator => 'qr',  braced => undef, separator => undef, _sections => 1, modifiers => 1 },
	'm'   => { operator => 'm',   braced => undef, separator => undef, _sections => 1, modifiers => 1 },
	's'   => { operator => 's',   braced => undef, separator => undef, _sections => 2, modifiers => 1 },
	'tr'  => { operator => 'tr',  braced => undef, separator => undef, _sections => 2, modifiers => 1 },

	# Y is the little-used variant of tr
	'y'   => { operator => 'y',   braced => undef, separator => undef, _sections => 2, modifiers => 1 },

	'/'   => { operator => undef, braced => 0,     separator => '/',   _sections => 1, modifiers => 1 },

	# Angle brackets quotes mean "readline(*FILEHANDLE)"
	'<'   => { operator => undef, braced => 1,     separator => undef, _sections => 1, },

	# The final ( and kind of depreciated ) "first match only" one is not
	# used yet, since I'm not sure on the context differences between
	# this and the trinary operator, but it's here for completeness.
	'?'   => { operator => undef, braced => 0,     separator => '?',   _sections => 1, modifiers => 1 },
);


sub new {
	my $class = shift;
	my $init  = defined $_[0]
		? shift
		: Carp::croak("::Full->new called without init string");

	# Create the token
	### This manual SUPER'ing ONLY works because none of
	### Token::Quote, Token::QuoteLike and Token::Regexp
	### implement a new function of their own.
	my $self = PPI::Token::new( $class, $init ) or return undef;

	# Do we have a prototype for the initializer? If so, add the extra fields
	my $options = $QUOTES{$init} or return $self->_error(
		"Unknown quote type '$init'"
	);
	foreach ( keys %$options ) {
		$self->{$_} = $options->{$_};
	}

	# Set up the modifiers hash if needed
	$self->{modifiers} = {} if $self->{modifiers};

	# Handle the special < base
	if ( $init eq '<' ) {
		$self->{sections}->[0] = Clone::clone( $SECTIONS{'<'} );
	}

	$self;
}

sub _fill {
	my $class = shift;
	my $t     = shift;
	my $self  = $t->{token}
		or Carp::croak("::Full->_fill called without current token");

	# Load in the operator stuff if needed
	if ( $self->{operator} ) {
		# In an operator based quote-like, handle the gap between the
		# operator and the opening separator.
		if ( substr( $t->{line}, $t->{line_cursor}, 1 ) =~ /\s/ ) {
			# Go past the gap
			my $gap = $self->_scan_quote_like_operator_gap( $t );
			return undef unless defined $gap;
			if ( ref $gap ) {
				# End of file
				$self->{content} .= $$gap;
				return 0;
			}
			$self->{content} .= $gap;
		}

		# The character we are now on is the separator. Capture,
		# and advance into the first section.
		my $sep = substr( $t->{line}, $t->{line_cursor}++, 1 );
		$self->{content} .= $sep;

		# Determine if these are normal or braced type sections
		if ( my $section = $SECTIONS{$sep} ) {
			$self->{braced}        = 1;
			$self->{sections}->[0] = Clone::clone($section);
		} else {
			$self->{braced}        = 0;
			$self->{separator}     = $sep;
		}
	}

	# Parse different based on whether we are normal or braced
	my $rv = $self->{braced}
		? $self->_fill_braced($t)
 		: $self->_fill_normal($t);
	return $rv if !$rv;

	# Return now unless it has modifiers ( i.e. s/foo//eieio )
	return 1 unless $self->{modifiers};

	# Check for modifiers
	my $char;
	my $len = 0;
	while ( ($char = substr( $t->{line}, $t->{line_cursor} + 1, 1 )) =~ /[^\W\d_]/ ) {
		$len++;
		$self->{content} .= $char;
		$self->{modifiers}->{lc $char} = 1;
		$t->{line_cursor}++;
	}
}

# Handle the content parsing path for normally separated
sub _fill_normal {
	my $self = shift;
	my $t    = shift;

	# Get the content up to the next separator
	my $string = $self->_scan_for_unescaped_character( $t, $self->{separator} );
	return undef unless defined $string;
	if ( ref $string ) {
		# End of file
		if ( length($$string) > 1 )  {
			# Complete the properties for the first section
			my $str = $$string;
			chop $str;
			$self->{sections}->[0] = {
				position => length($self->{content}),
				size     => length($$string) - 1,
				type     => "$self->{separator}$self->{separator}",
			};
			$self->{_sections} = 1;
		} else {
			# No sections at all
			$self->{sections}  = [ ];
			$self->{_sections} = 0;
		}
		$self->{content} .= $$string;
		return 0;
	}

	# Complete the properties of the first section
	$self->{sections}->[0] = {
		position => length $self->{content},
		size     => length($string) - 1,
		type     => "$self->{separator}$self->{separator}",
	};
	$self->{content} .= $string;

	# We are done if there is only one section
	return 1 if $self->{_sections} == 1;

	# There are two sections.

	# Advance into the next section
	$t->{line_cursor}++;

	# Get the content up to the end separator
	$string = $self->_scan_for_unescaped_character( $t, $self->{separator} );
	return undef unless defined $string;
	if ( ref $string ) {
		# End of file
		if ( length($$string) > 1 )  {
			# Complete the properties for the second section
			my $str = $$string;
			chop $str;
			$self->{sections}->[1] = {
				position => length($self->{content}),
				size     => length($$string) - 1,
				type     => "$self->{separator}$self->{separator}",
			};
		} else {
			# No sections at all
			$self->{_sections} = 1;
		}
		$self->{content} .= $$string;
		return 0;
	}

	# Complete the properties of the second section
	$self->{sections}->[1] = {
		position => length($self->{content}),
		size     => length($string) - 1
	};
	$self->{content} .= $string;

	1;
}

# Handle content parsing for matching brace separated
sub _fill_braced {
	my $self = shift;
	my $t    = shift;

	# Get the content up to the close character
	my $section   = $self->{sections}->[0];
	my $brace_str = $self->_scan_for_brace_character( $t, $section->{_close} );
	return undef unless defined $brace_str;
	if ( ref $brace_str ) {
		# End of file
		if ( length($$brace_str) > 1 )  {
			# Complete the properties for the first section
			my $str = $$brace_str;
			chop $str;
			$self->{sections}->[0] = {
				position => length($self->{content}),
				size     => length($$brace_str) - 1,
				type     => $section->{type},
			};
			$self->{_sections} = 1;
		} else {
			# No sections at all
			$self->{sections}  = [ ];
			$self->{_sections} = 0;
		}
		$self->{content} .= $$brace_str;
		return 0;
	}

	# Complete the properties of the first section
	$section->{position} = length $self->{content};
	$section->{size}     = length($brace_str) - 1;
	$self->{content} .= $brace_str;
	delete $section->{_close};

	# We are done if there is only one section
	return 1 if $self->{_sections} == 1;

	# There are two sections.

	# Is there a gap between the sections.
	my $char = substr( $t->{line}, ++$t->{line_cursor}, 1 );
	if ( $char =~ /\s/ ) {
		# Go past the gap
		my $gap_str = $self->_scan_quote_like_operator_gap( $t );
		return undef unless defined $gap_str;
		if ( ref $gap_str ) {
			# End of file
			$self->{content} .= $$gap_str;
			return 0;
		}
		$self->{content} .= $gap_str;
		$char = substr( $t->{line}, $t->{line_cursor}, 1 );
	}

	$section = $SECTIONS{$char};

	if ( $section ) {
		# It's a brace

		# Initialize the second section
		$self->{content} .= $char;
		$section = { %$section };

		# Advance into the second section
		$t->{line_cursor}++;

		# Get the content up to the close character
		$brace_str = $self->_scan_for_brace_character( $t, $section->{_close} );
		return undef unless defined $brace_str;
		if ( ref $brace_str ) {
			# End of file
			if ( length($$brace_str) > 1 )  {
				# Complete the properties for the second section
				my $str = $$brace_str;
				chop $str;
				$self->{sections}->[1] = {
					position => length($self->{content}),
					size     => length($$brace_str) - 1,
					type     => $section->{type},
				};
				$self->{_sections} = 2;
			} else {
				# No sections at all
				$self->{_sections} = 1;
			}
			$self->{content} .= $$brace_str;
			return 0;
		} else {
			# Complete the properties for the second section
			$self->{sections}->[1] = {
				position => length($self->{content}),
				size     => length($brace_str) - 1,
				type     => $section->{type},
			};
			$self->{content} .= $brace_str;
		}
	} elsif ( $char =~ m/ \A [^\w\s] \z /smx ) {
		# It is some other delimiter (weird, but possible)

		# Add the delimiter to the content.
		$self->{content} .= $char;

		# Advance into the next section
		$t->{line_cursor}++;

		# Get the content up to the end separator
		my $string = $self->_scan_for_unescaped_character( $t, $char );
		return undef unless defined $string;
		if ( ref $string ) {
			# End of file
			if ( length($$string) > 1 )  {
				# Complete the properties for the second section
				my $str = $$string;
				chop $str;
				$self->{sections}->[1] = {
					position => length($self->{content}),
					size     => length($$string) - 1,
					type     => "$char$char",
				};
			} else {
				# Only the one section
				$self->{_sections} = 1;
			}
			$self->{content} .= $$string;
			return 0;
		}

		# Complete the properties of the second section
		$self->{sections}->[1] = {
			position => length($self->{content}),
			size     => length($string) - 1,
			type     => "$char$char", 
		};
		$self->{content} .= $string;

	} else {

		# Error, it has to be a delimiter of some sort.
		# Although this will result in a REALLY illegal regexp,
		# we allow it anyway.

		# Create a null second section
		$self->{sections}->[1] = {
			position => length($self->{content}),
			size     => 0,
			type     => '',
		};

		# Attach an error to the token and move on
		$self->{_error} = "No second section of regexp, or does not start with a balanced character";

		# Roll back the cursor one char and return signalling end of regexp
		$t->{line_cursor}--;
		return 0;
	}

	1;
}





#####################################################################
# Additional methods to find out about the quote

# In a scalar context, get the number of sections
# In an array context, get the section information
sub _sections {
	wantarray ? @{$_[0]->{sections}} : scalar @{$_[0]->{sections}}
}

# Get a section's content
sub _section_content {
	my $self = shift;
	my $i    = shift;
	$self->{sections} or return;
	my $section = $self->{sections}->[$i] or return;
	return substr( $self->content, $section->{position}, $section->{size} );
}

# Get the modifiers if any.
# In list context, return the modifier hash.
# In scalar context, clone the hash and return a reference to it.
# If there are no modifiers, simply return.
sub _modifiers {
	my $self = shift;
	$self->{modifiers} or return;
	wantarray and return %{ $self->{modifiers} };
	return +{ %{ $self->{modifiers} } };
}

# Get the delimiters, or at least give it a good try to get them.
sub _delimiters {
	my $self = shift;
	$self->{sections} or return;
	my @delims;
	foreach my $sect ( @{ $self->{sections} } ) {
		if ( exists $sect->{type} ) {
			push @delims, $sect->{type};
		} else {
			my $content = $self->content;
			push @delims,
			substr( $content, $sect->{position} - 1, 1 ) .
			substr( $content, $sect->{position} + $sect->{size}, 1 );
		}
	}
	return @delims;
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
