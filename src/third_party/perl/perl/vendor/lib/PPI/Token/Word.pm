package PPI::Token::Word;

=pod

=head1 NAME

PPI::Token::Word - The generic "word" Token

=head1 INHERITANCE

  PPI::Token::Word
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

A C<PPI::Token::Word> object is a PPI-specific representation of several
different types of word-like things, and is one of the most common Token
classes found in typical documents.

Specifically, it includes not only barewords, but also any other valid
Perl identifier including non-operator keywords and core functions, and
any include C<::> separators inside it, as long as it fits the
format of a class, function, etc.

=head1 METHODS

There are no methods available for C<PPI::Token::Word> beyond those
provided by its L<PPI::Token> and L<PPI::Element> parent
classes.

We expect to add additional methods to help further resolve a Word as
a function, method, etc over time.  If you need such a thing right
now, look at L<Perl::Critic::Utils>.

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA %OPERATOR %QUOTELIKE};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';

	# Copy in OPERATOR from PPI::Token::Operator
	*OPERATOR  = *PPI::Token::Operator::OPERATOR;

	%QUOTELIKE = (
		'q'  => 'Quote::Literal',
		'qq' => 'Quote::Interpolate',
		'qx' => 'QuoteLike::Command',
		'qw' => 'QuoteLike::Words',
		'qr' => 'QuoteLike::Regexp',
		'm'  => 'Regexp::Match',
		's'  => 'Regexp::Substitute',
		'tr' => 'Regexp::Transliterate',
		'y'  => 'Regexp::Transliterate',
	);
}

=pod

=head2 literal

Returns the value of the Word as a string.  This assumes (often
incorrectly) that the Word is a bareword and not a function, method,
keyword, etc.  This differs from C<content> because C<Foo'Bar> expands
to C<Foo::Bar>.

=begin testing literal 9

my @pairs = (
	"F",          'F',
	"Foo::Bar",   'Foo::Bar',
	"Foo'Bar",    'Foo::Bar',
);
while ( @pairs ) {
	my $from  = shift @pairs;
	my $to    = shift @pairs;
	my $doc   = PPI::Document->new( \"$from;" );
	isa_ok( $doc, 'PPI::Document' );
	my $word = $doc->find_first('Token::Word');
	isa_ok( $word, 'PPI::Token::Word' );
	is( $word->literal, $to, "The source $from becomes $to ok" );
}

=end testing

=cut

sub literal {
	my $self = shift;
	my $word = $self->content;

	# Expand Foo'Bar to Foo::Bar
	$word =~ s/\'/::/g;

	return $word;
}

=pod

=head2 method_call

Answers whether this is the name of a method in a method call. Returns true if
yes, false if no, and nothing if unknown.

=begin testing method_call 24

my $Document = PPI::Document->new(\<<'END_PERL');
indirect $foo;
indirect_class_with_colon Foo::;
$bar->method_with_parentheses;
print SomeClass->method_without_parentheses + 1;
sub_call();
$baz->chained_from->chained_to;
a_first_thing a_middle_thing a_last_thing;
(first_list_element, second_list_element, third_list_element);
first_comma_separated_word, second_comma_separated_word, third_comma_separated_word;
single_bareword_statement;
{ bareword_no_semicolon_end_of_block }
$buz{hash_key};
fat_comma_left_side => $thingy;
END_PERL

isa_ok( $Document, 'PPI::Document' );
my $words = $Document->find('Token::Word');
is( scalar @{$words}, 23, 'Found the 23 test words' );
my %words = map { $_ => $_ } @{$words};
is(
	scalar $words{indirect}->method_call,
	undef,
	'Indirect notation is unknown.',
);
is(
	scalar $words{indirect_class_with_colon}->method_call,
	1,
	'Indirect notation with following word ending with colons is true.',
);
is(
	scalar $words{method_with_parentheses}->method_call,
	1,
	'Method with parentheses is true.',
);
is(
	scalar $words{method_without_parentheses}->method_call,
	1,
	'Method without parentheses is true.',
);
is(
	scalar $words{print}->method_call,
	undef,
	'Plain print is unknown.',
);
is(
	scalar $words{SomeClass}->method_call,
	undef,
	'Class in class method call is unknown.',
);
is(
	scalar $words{sub_call}->method_call,
	0,
	'Subroutine call is false.',
);
is(
	scalar $words{chained_from}->method_call,
	1,
	'Method that is chained from is true.',
);
is(
	scalar $words{chained_to}->method_call,
	1,
	'Method that is chained to is true.',
);
is(
	scalar $words{a_first_thing}->method_call,
	undef,
	'First bareword is unknown.',
);
is(
	scalar $words{a_middle_thing}->method_call,
	undef,
	'Bareword in the middle is unknown.',
);
is(
	scalar $words{a_last_thing}->method_call,
	0,
	'Bareword at the end is false.',
);
foreach my $false_word (
	qw<
		first_list_element second_list_element third_list_element
		first_comma_separated_word second_comma_separated_word third_comma_separated_word
		single_bareword_statement
		bareword_no_semicolon_end_of_block
		hash_key
		fat_comma_left_side
	>
) {
	is(
		scalar $words{$false_word}->method_call,
		0,
		"$false_word is false.",
	);
}

=end testing

=cut

sub method_call {
	my $self = shift;

	my $previous = $self->sprevious_sibling;
	if (
		$previous
		and
		$previous->isa('PPI::Token::Operator')
		and
		$previous->content eq '->'
	) {
		return 1;
	}

	my $snext = $self->snext_sibling;
	return 0 unless $snext;

	if (
		$snext->isa('PPI::Structure::List')
		or
		$snext->isa('PPI::Token::Structure')
		or
		$snext->isa('PPI::Token::Operator')
		and (
			$snext->content eq ','
			or
			$snext->content eq '=>'
		)
	) {
		return 0;
	}

	if (
		$snext->isa('PPI::Token::Word')
		and
		$snext->content =~ m< \w :: \z >xms
	) {
		return 1;
	}

	return;
}

=begin testing __TOKENIZER__on_char 27

my $Document = PPI::Document->new(\<<'END_PERL');
$foo eq'bar';
$foo ne'bar';
$foo ge'bar';
$foo le'bar';
$foo gt'bar';
$foo lt'bar';
END_PERL

isa_ok( $Document, 'PPI::Document' );
my $words = $Document->find('Token::Operator');
is( scalar @{$words}, 6, 'Found the 6 test operators' );

is( $words->[0], 'eq', q{$foo eq'bar'} );
is( $words->[1], 'ne', q{$foo ne'bar'} );
is( $words->[2], 'ge', q{$foo ge'bar'} );
is( $words->[3], 'le', q{$foo le'bar'} );
is( $words->[4], 'gt', q{$foo ht'bar'} );
is( $words->[5], 'lt', q{$foo lt'bar'} );

$Document = PPI::Document->new(\<<'END_PERL');
q'foo';
qq'foo';
END_PERL

isa_ok( $Document, 'PPI::Document' );
$words = $Document->find('Token::Quote');
is( scalar @{$words}, 2, 'Found the 2 test quotes' );

is( $words->[0], q{q'foo'}, q{q'foo'} );
is( $words->[1], q{qq'foo'}, q{qq'foo'} );

$Document = PPI::Document->new(\<<'END_PERL');
qx'foo';
qw'foo';
qr'foo';
END_PERL

isa_ok( $Document, 'PPI::Document' );
$words = $Document->find('Token::QuoteLike');
is( scalar @{$words}, 3, 'Found the 3 test quotelikes' );

is( $words->[0], q{qx'foo'}, q{qx'foo'} );
is( $words->[1], q{qw'foo'}, q{qw'foo'} );
is( $words->[2], q{qr'foo'}, q{qr'foo'} );

$Document = PPI::Document->new(\<<'END_PERL');
m'foo';
s'foo'bar';
tr'fo'ba';
y'fo'ba';
END_PERL

isa_ok( $Document, 'PPI::Document' );
$words = $Document->find('Token::Regexp');
is( scalar @{$words}, 4, 'Found the 4 test quotelikes' );

is( $words->[0], q{m'foo'},     q{m'foo'} );
is( $words->[1], q{s'foo'bar'}, q{s'foo'bar'} );
is( $words->[2], q{tr'fo'ba'},  q{tr'fo'ba'} );
is( $words->[3], q{y'fo'ba'},   q{y'fo'ba'} );

$Document = PPI::Document->new(\<<'END_PERL');
pack'H*',$data;
unpack'H*',$data;
END_PERL

isa_ok( $Document, 'PPI::Document' );
$words = $Document->find('Token::Word');
is( scalar @{$words}, 2, 'Found the 2 test words' );

is( $words->[0], 'pack', q{pack'H*',$data} );
is( $words->[1], 'unpack', q{unpack'H*',$data} );

=end testing

=cut

my %backoff = map { $_ => 1 } qw{
    eq ne ge le gt lt
    q qq qx qw qr m s tr y
    pack unpack
};

sub __TOKENIZER__on_char {
	my $class = shift;
	my $t     = shift;

	# Suck in till the end of the bareword
	my $rest = substr( $t->{line}, $t->{line_cursor} );
	if ( $rest =~ /^(\w+(?:(?:\'|::)\w+)*(?:::)?)/ ) {
		my $word = $1;
		# Special Case: If we accidentally treat eq'foo' like
		# the word "eq'foo", then just make 'eq' (or whatever
		# else is in the %backoff hash.
		if ( $word =~ /^(\w+)'/ && $backoff{$1} ) {
		    $word = $1;
		}
		$t->{token}->{content} .= $word;
		$t->{line_cursor} += length $word;

	}

	# We might be a subroutine attribute.
	my $tokens = $t->_previous_significant_tokens(1);
	if ( $tokens and $tokens->[0]->{_attribute} ) {
		$t->{class} = $t->{token}->set_class( 'Attribute' );
		return $t->{class}->__TOKENIZER__commit( $t );
	}

	# Check for a quote like operator
	my $word = $t->{token}->{content};
	if ( $QUOTELIKE{$word} and ! $class->__TOKENIZER__literal($t, $word, $tokens) ) {
		$t->{class} = $t->{token}->set_class( $QUOTELIKE{$word} );
		return $t->{class}->__TOKENIZER__on_char( $t );
	}

	# Or one of the word operators
	if ( $OPERATOR{$word} and ! $class->__TOKENIZER__literal($t, $word, $tokens) ) {
	 	$t->{class} = $t->{token}->set_class( 'Operator' );
 		return $t->_finalize_token->__TOKENIZER__on_char( $t );
	}

	# Unless this is a simple identifier, at this point
	# it has to be a normal bareword
	if ( $word =~ /\:/ ) {
		return $t->_finalize_token->__TOKENIZER__on_char( $t );
	}

	# If the NEXT character in the line is a colon, this
	# is a label.
	my $char = substr( $t->{line}, $t->{line_cursor}, 1 );
	if ( $char eq ':' ) {
		$t->{token}->{content} .= ':';
		$t->{line_cursor}++;
		$t->{class} = $t->{token}->set_class( 'Label' );

	# If not a label, '_' on its own is the magic filehandle
	} elsif ( $word eq '_' ) {
		$t->{class} = $t->{token}->set_class( 'Magic' );

	}

	# Finalise and process the character again
	$t->_finalize_token->__TOKENIZER__on_char( $t );
}



# We are committed to being a bareword.
# Or so we would like to believe.
sub __TOKENIZER__commit {
	my ($class, $t) = @_;

	# Our current position is the first character of the bareword.
	# Capture the bareword.
	my $rest = substr( $t->{line}, $t->{line_cursor} );
	unless ( $rest =~ /^((?!\d)\w+(?:(?:\'|::)\w+)*(?:::)?)/ ) {
		# Programmer error
		die "Fatal error... regex failed to match in '$rest' when expected";
	}

	# Special Case: If we accidentally treat eq'foo' like the word "eq'foo",
	# then unwind it and just make it 'eq' (or the other stringy comparitors)
	my $word = $1;
	if ( $word =~ /^(\w+)'/ && $backoff{$1} ) {
	    $word = $1;
	}

	# Advance the position one after the end of the bareword
	$t->{line_cursor} += length $word;

	# We might be a subroutine attribute.
	my $tokens = $t->_previous_significant_tokens(1);
	if ( $tokens and $tokens->[0]->{_attribute} ) {
		$t->_new_token( 'Attribute', $word );
		return ($t->{line_cursor} >= $t->{line_length}) ? 0
			: $t->{class}->__TOKENIZER__on_char($t);
	}

	# Check for the end of the file
	if ( $word eq '__END__' ) {
		# Create the token for the __END__ itself
		$t->_new_token( 'Separator', $1 );
		$t->_finalize_token;

		# Move into the End zone (heh)
		$t->{zone} = 'PPI::Token::End';

		# Add the rest of the line as a comment, and a whitespace newline
		# Anything after the __END__ on the line is "ignored". So we must
		# also ignore it, by turning it into a comment.
		$rest = substr( $t->{line}, $t->{line_cursor} );
		$t->{line_cursor} = length $t->{line};
		if ( $rest =~ /\n$/ ) {
			chomp $rest;
			$t->_new_token( 'Comment', $rest ) if length $rest;
			$t->_new_token( 'Whitespace', "\n" );
		} else {
			$t->_new_token( 'Comment', $rest ) if length $rest;
		}
		$t->_finalize_token;

		return 0;
	}

	# Check for the data section
	if ( $word eq '__DATA__' ) {
		# Create the token for the __DATA__ itself
		$t->_new_token( 'Separator', "$1" );
		$t->_finalize_token;

		# Move into the Data zone
		$t->{zone} = 'PPI::Token::Data';

		# Add the rest of the line as the Data token
		$rest = substr( $t->{line}, $t->{line_cursor} );
		$t->{line_cursor} = length $t->{line};
		if ( $rest =~ /\n$/ ) {
			chomp $rest;
			$t->_new_token( 'Comment', $rest ) if length $rest;
			$t->_new_token( 'Whitespace', "\n" );
		} else {
			$t->_new_token( 'Comment', $rest ) if length $rest;
		}
		$t->_finalize_token;

		return 0;
	}

	my $token_class;
	if ( $word =~ /\:/ ) {
		# Since its not a simple identifier...
		$token_class = 'Word';

	} elsif ( $class->__TOKENIZER__literal($t, $word, $tokens) ) {
		$token_class = 'Word';

	} elsif ( $QUOTELIKE{$word} ) {
		# Special Case: A Quote-like operator
		$t->_new_token( $QUOTELIKE{$word}, $word );
		return ($t->{line_cursor} >= $t->{line_length}) ? 0
			: $t->{class}->__TOKENIZER__on_char( $t );

	} elsif ( $OPERATOR{$word} ) {
		# Word operator
		$token_class = 'Operator';

	} else {
		# If the next character is a ':' then its a label...
		my $string = substr( $t->{line}, $t->{line_cursor} );
		if ( $string =~ /^(\s*:)(?!:)/ ) {
			if ( $tokens and $tokens->[0]->{content} eq 'sub' ) {
				# ... UNLESS its after 'sub' in which
				# case it is a sub name and an attribute
				# operator.
				# We COULD have checked this at the top
				# level of checks, but this would impose
				# an additional performance per-word
				# penalty, and every other case where the
				# attribute operator doesn't directly
				# touch the object name already works.
				$token_class = 'Word';
			} else {
				$word .= $1;
				$t->{line_cursor} += length($1);
				$token_class = 'Label';
			}
		} elsif ( $word eq '_' ) {
			$token_class = 'Magic';
		} else {
			$token_class = 'Word';
		}
	}

	# Create the new token and finalise
	$t->_new_token( $token_class, $word );
	if ( $t->{line_cursor} >= $t->{line_length} ) {
		# End of the line
		$t->_finalize_token;
		return 0;
	}
	$t->_finalize_token->__TOKENIZER__on_char($t);
}

# Is the word in a "forced" context, and thus cannot be either an
# operator or a quote-like thing. This version is only useful
# during tokenization.
sub __TOKENIZER__literal {
	my ($class, $t, $word, $tokens) = @_;

	# Is this a forced-word context?
	# i.e. Would normally be seen as an operator.
	unless ( $QUOTELIKE{$word} or $PPI::Token::Operator::OPERATOR{$word} ) {
		return '';
	}

	# Check the cases when we have previous tokens
	my $rest = substr( $t->{line}, $t->{line_cursor} );
	if ( $tokens ) {
		my $token = $tokens->[0] or return '';

		# We are forced if we are a method name
		return 1 if $token->{content} eq '->';

		# We are forced if we are a sub name
		return 1 if $token->isa('PPI::Token::Word') && $token->{content} eq 'sub';

		# If we are contained in a pair of curly braces,
		# we are probably a bareword hash key
		if ( $token->{content} eq '{' and $rest =~ /^\s*\}/ ) {
			return 1;
		}
	}

	# In addition, if the word is followed by => it is probably
	# also actually a word and not a regex.
	if ( $rest =~ /^\s*=>/ ) {
		return 1;
	}

	# Otherwise we probably arn't forced
	'';
}

1;

=pod

=head1 TO DO

- Add C<function>, C<method> etc detector methods

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
