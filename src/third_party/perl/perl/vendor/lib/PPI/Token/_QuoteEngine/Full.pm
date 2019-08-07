package PPI::Token::_QuoteEngine::Full;

# Full quote engine

use strict;
use Clone                    ();
use Carp                     ();
use PPI::Token::_QuoteEngine ();

use vars qw{$VERSION @ISA %quotes %sections};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token::_QuoteEngine';

	# Prototypes for the different braced sections
	%sections = (
		'(' => { type => '()', _close => ')' },
		'<' => { type => '<>', _close => '>' },
		'[' => { type => '[]', _close => ']' },
		'{' => { type => '{}', _close => '}' },
	);

	# For each quote type, the extra fields that should be set.
	# This should give us faster initialization.
	%quotes = (
		'q'   => { operator => 'q',   braced => undef, separator => undef, _sections => 1 },
		'qq'  => { operator => 'qq',  braced => undef, separator => undef, _sections => 1 },
		'qx'  => { operator => 'qx',  braced => undef, separator => undef, _sections => 1 },
		'qw'  => { operator => 'qw',  braced => undef, separator => undef, _sections => 1 },
		'qr'  => { operator => 'qr',  braced => undef, separator => undef, _sections => 1, modifiers => 1 },
		'm'   => { operator => 'm',   braced => undef, separator => undef, _sections => 1, modifiers => 1 },
		's'   => { operator => 's',   braced => undef, separator => undef, _sections => 2, modifiers => 1 },
		'tr'  => { operator => 'tr',  braced => undef, separator => undef, _sections => 2, modifiers => 1 },

		# Y is the little used varient of tr
		'y'   => { operator => 'y',   braced => undef, separator => undef, _sections => 2, modifiers => 1 },

		'/'   => { operator => undef, braced => 0,     separator => '/',   _sections => 1, modifiers => 1 },

		# Angle brackets quotes mean "readline(*FILEHANDLE)"
		'<'   => { operator => undef, braced => 1,     separator => undef, _sections => 1, },

		# The final ( and kind of depreciated ) "first match only" one is not
		# used yet, since I'm not sure on the context differences between
		# this and the trinary operator, but its here for completeness.
		'?'   => { operator => undef, braced => 0,     separator => '?',   _sections => 1, modifiers => 1 },
	);
}

=pod

=begin testing new 90

# Verify that Token::Quote, Token::QuoteLike and Token::Regexp
# do not have ->new functions
my $RE_SYMBOL  = qr/\A(?!\d)\w+\z/;
foreach my $name ( qw{Token::Quote Token::QuoteLike Token::Regexp} ) {
	no strict 'refs';
	my @functions = sort
		grep { defined &{"${name}::$_"} }
		grep { /$RE_SYMBOL/o }
		keys %{"PPI::${name}::"};
	is( scalar(grep { $_ eq 'new' } @functions), 0,
		"$name does not have a new function" );
}

# This primarily to ensure that qw() with non-balanced types
# are treated the same as those with balanced types.
SCOPE: {
	my @seps   = ( undef, undef, '/', '#', ','  );
	my @types  = ( '()', '<>', '//', '##', ',,' );
	my @braced = ( qw{ 1 1 0 0 0 } );
	my $i      = 0;
	for my $q ('qw()', 'qw<>', 'qw//', 'qw##', 'qw,,') {
		my $d = PPI::Document->new(\$q);
		my $o = $d->{children}->[0]->{children}->[0];
		my $s = $o->{sections}->[0];
		is( $o->{operator},  'qw',      "$q correct operator"  );
		is( $o->{_sections}, 1,         "$q correct _sections" );
		is( $o->{braced}, $braced[$i],  "$q correct braced"    );
		is( $o->{separator}, $seps[$i], "$q correct seperator" );
		is( $o->{content},   $q,        "$q correct content"   );
		is( $s->{position},  3,         "$q correct position"  );
		is( $s->{type}, $types[$i],     "$q correct type"      );
		is( $s->{size},      0,         "$q correct size"      );
		$i++;
	}
}

SCOPE: {
	my @stuff  = ( qw-( ) < > / / -, '#', '#', ',',',' );
	my @seps   = ( undef, undef, '/', '#', ','  );
	my @types  = ( '()', '<>', '//', '##', ',,' );
	my @braced = ( qw{ 1 1 0 0 0 } );
	my @secs   = ( qw{ 1 1 0 0 0 } );
	my $i      = 0;
	while ( @stuff ) {
		my $opener = shift @stuff;
		my $closer = shift @stuff;
		my $d = PPI::Document->new(\"qw$opener");
		my $o = $d->{children}->[0]->{children}->[0];
		my $s = $o->{sections}->[0];
		is( $o->{operator},  'qw',        "qw$opener correct operator"  );
		is( $o->{_sections}, $secs[$i],   "qw$opener correct _sections" );
		is( $o->{braced}, $braced[$i],    "qw$opener correct braced"    );
		is( $o->{separator}, $seps[$i],   "qw$opener correct seperator" );
		is( $o->{content},   "qw$opener", "qw$opener correct content"   );
		if ( $secs[$i] ) {
			is( $s->{type}, "$opener$closer", "qw$opener correct type"      );
		}
		$i++;
	}
}

SCOPE: {
	foreach (
		[ '/foo/i',       'foo', undef, { i => 1 }, [ '//' ] ],
		[ 'm<foo>x',      'foo', undef, { x => 1 }, [ '<>' ] ],
		[ 's{foo}[bar]g', 'foo', 'bar', { g => 1 }, [ '{}', '[]' ] ],
		[ 'tr/fo/ba/',    'fo',  'ba',  {},         [ '//', '//' ] ],
		[ 'qr{foo}smx',   'foo', undef, { s => 1, m => 1, x => 1 },
							    [ '{}' ] ],
	) {
		my ( $code, $match, $subst, $mods, $delims ) = @{ $_ };
		my $doc = PPI::Document->new( \$code );
		$doc or warn "'$code' did not create a document";
		my $obj = $doc->child( 0 )->child( 0 );
		is( $obj->_section_content( 0 ), $match, "$code correct match" );
		is( $obj->_section_content( 1 ), $subst, "$code correct subst" );
		is_deeply( { $obj->_modifiers() }, $mods, "$code correct modifiers" );
		is_deeply( [ $obj->_delimiters() ], $delims, "$code correct delimiters" );
	}
}

=end testing

=cut

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

	# Do we have a prototype for the intializer? If so, add the extra fields
	my $options = $quotes{$init} or return $self->_error(
		"Unknown quote type '$init'"
	);
	foreach ( keys %$options ) {
		$self->{$_} = $options->{$_};
	}

	# Set up the modifiers hash if needed
	$self->{modifiers} = {} if $self->{modifiers};

	# Handle the special < base
	if ( $init eq '<' ) {
		$self->{sections}->[0] = Clone::clone( $sections{'<'} );
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
		if ( my $section = $sections{$sep} ) {
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

# Handle the content parsing path for normally seperated
sub _fill_normal {
	my $self = shift;
	my $t    = shift;

	# Get the content up to the next separator
	my $string = $self->_scan_for_unescaped_character( $t, $self->{separator} );
	return undef unless defined $string;
	if ( ref $string ) {
		# End of file
		$self->{content} .= $$string;
		if ( length($$string) > 1 )  {
			# Complete the properties for the first section
			my $str = $$string;
			chop $str;
			$self->{sections}->[0] = {
				position => length($self->{content}),
				size     => length($string),
				type     => "$self->{separator}$self->{separator}",
			};
		} else {
			# No sections at all
			$self->{_sections} = 0;
		}
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

# Handle content parsing for matching crace seperated
sub _fill_braced {
	my $self = shift;
	my $t    = shift;

	# Get the content up to the close character
	my $section   = $self->{sections}->[0];
	my $brace_str = $self->_scan_for_brace_character( $t, $section->{_close} );
	return undef unless defined $brace_str;
	if ( ref $brace_str ) {
		# End of file
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

	$section = $sections{$char};

	if ( $section ) {
		# It's a brace

		# Initialize the second section
		$self->{content} .= $char;
		$section = $self->{sections}->[1] = { %$section };

		# Advance into the second region
		$t->{line_cursor}++;
		$section->{position} = length($self->{content});
		$section->{size}     = 0;

		# Get the content up to the close character
		$brace_str = $self->_scan_for_brace_character( $t, $section->{_close} );
		return undef unless defined $brace_str;
		if ( ref $brace_str ) {
			# End of file
			$self->{content} .= $$brace_str;
			$section->{size} = length($$brace_str);
			delete $section->{_close};
			return 0;
		} else {
			# Complete the properties for the second section
			$self->{content} .= $brace_str;
			$section->{size} = length($brace_str) - 1;
			delete $section->{_close};
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
sub _sections { wantarray ? @{$_[0]->{sections}} : scalar @{$_[0]->{sections}} }

# Get a section's content
sub _section_content {
	my ( $self, $inx ) = @_;
	$self->{sections} or return;
	my $sect = $self->{sections}[$inx] or return;
	return substr $self->content(), $sect->{position}, $sect->{size};
}

# Get the modifiers if any.
# In list context, return the modifier hash.
# In scalar context, clone the hash and return a reference to it.
# If there are no modifiers, simply return.
sub _modifiers {
	my ( $self ) = @_;
	$self->{modifiers} or return;
	wantarray and return %{ $self->{modifiers} };
	return +{ %{ $self->{modifiers} } };
}

# Get the delimiters, or at least give it a good try to get them.
sub _delimiters {
	my ( $self ) = @_;
	$self->{sections} or return;
	my @delims;
	foreach my $sect ( @{ $self->{sections} } ) {
		if ( exists $sect->{type} ) {
			push @delims, $sect->{type};
		} else {
			my $content = $self->content();
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
