package PPI::Tokenizer;

=pod

=head1 NAME

PPI::Tokenizer - The Perl Document Tokenizer

=head1 SYNOPSIS

  # Create a tokenizer for a file, array or string
  $Tokenizer = PPI::Tokenizer->new( 'filename.pl' );
  $Tokenizer = PPI::Tokenizer->new( \@lines       );
  $Tokenizer = PPI::Tokenizer->new( \$source      );
  
  # Return all the tokens for the document
  my $tokens = $Tokenizer->all_tokens;
  
  # Or we can use it as an iterator
  while ( my $Token = $Tokenizer->get_token ) {
  	print "Found token '$Token'\n";
  }
  
  # If we REALLY need to manually nudge the cursor, you
  # can do that to (The lexer needs this ability to do rollbacks)
  $is_incremented = $Tokenizer->increment_cursor;
  $is_decremented = $Tokenizer->decrement_cursor;

=head1 DESCRIPTION

PPI::Tokenizer is the class that provides Tokenizer objects for use in
breaking strings of Perl source code into Tokens.

By the time you are reading this, you probably need to know a little
about the difference between how perl parses Perl "code" and how PPI
parsers Perl "documents".

"perl" itself (the interpreter) uses a heavily modified lex specification
to specify its parsing logic, maintains several types of state as it
goes, and incrementally tokenizes, lexes AND EXECUTES at the same time.

In fact, it is provably impossible to use perl's parsing method without
simultaneously executing code. A formal mathematical proof has been
published demonstrating the method.

This is where the truism "Only perl can parse Perl" comes from.

PPI uses a completely different approach by abandoning the (impossible)
ability to parse Perl the same way that the interpreter does, and instead
parsing the source as a document, using a document structure independantly
derived from the Perl documentation and approximating the perl interpreter
interpretation as closely as possible.

It was touch and go for a long time whether we could get it close enough,
but in the end it turned out that it could be done.

In this approach, the tokenizer C<PPI::Tokenizer> is implemented separately
from the lexer L<PPI::Lexer>.

The job of C<PPI::Tokenizer> is to take pure source as a string and break it
up into a stream/set of tokens, and contains most of the "black magic" used
in PPI. By comparison, the lexer implements a relatively straight forward
tree structure, and has an implementation that is uncomplicated (compared
to the insanity in the tokenizer at least).

The Tokenizer uses an immense amount of heuristics, guessing and cruft,
supported by a very B<VERY> flexible internal API, but fortunately it was
possible to largely encapsulate the black magic, so there is not a lot that
gets exposed to people using the C<PPI::Tokenizer> itself.

=head1 METHODS

Despite the incredible complexity, the Tokenizer itself only exposes a
relatively small number of methods, with most of the complexity implemented
in private methods.

=cut

# Make sure everything we need is loaded so
# we don't have to go and load all of PPI.
use strict;
use Params::Util    qw{_INSTANCE _SCALAR0 _ARRAY0};
use List::MoreUtils ();
use PPI::Util       ();
use PPI::Element    ();
use PPI::Token      ();
use PPI::Exception  ();
use PPI::Exception::ParserRejection ();

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.215';
}





#####################################################################
# Creation and Initialization

=pod

=head2 new $file | \@lines | \$source

The main C<new> constructor creates a new Tokenizer object. These
objects have no configuration parameters, and can only be used once,
to tokenize a single perl source file.

It takes as argument either a normal scalar containing source code,
a reference to a scalar containing source code, or a reference to an
ARRAY containing newline-terminated lines of source code.

Returns a new C<PPI::Tokenizer> object on success, or throws a
L<PPI::Exception> exception on error.

=cut

sub new {
	my $class = ref($_[0]) || $_[0];

	# Create the empty tokenizer struct
	my $self = bless {
		# Source code
		source       => undef,
		source_bytes => undef,

		# Line buffer
		line         => undef,
		line_length  => undef,
		line_cursor  => undef,
		line_count   => 0,

		# Parse state
		token        => undef,
		class        => 'PPI::Token::BOM',
		zone         => 'PPI::Token::Whitespace',

		# Output token buffer
		tokens       => [],
		token_cursor => 0,
		token_eof    => 0,

		# Perl 6 blocks
		perl6        => [],
	}, $class;

	if ( ! defined $_[1] ) {
		# We weren't given anything
		PPI::Exception->throw("No source provided to Tokenizer");

	} elsif ( ! ref $_[1] ) {
		my $source = PPI::Util::_slurp($_[1]);
		if ( ref $source ) {
			# Content returned by reference
			$self->{source} = $$source;
		} else {
			# Errors returned as a string
			return( $source );
		}

	} elsif ( _SCALAR0($_[1]) ) {
		$self->{source} = ${$_[1]};

	} elsif ( _ARRAY0($_[1]) ) {
		$self->{source} = join '', map { "\n" } @{$_[1]};

	} else {
		# We don't support whatever this is
		PPI::Exception->throw(ref($_[1]) . " is not supported as a source provider");
	}

	# We can't handle a null string
	$self->{source_bytes} = length $self->{source};
	if ( $self->{source_bytes} > 1048576 ) {
		# Dammit! It's ALWAYS the "Perl" modules larger than a
		# meg that seems to blow up the Tokenizer/Lexer.
		# Nobody actually writes real programs larger than a meg
		# Perl::Tidy (the largest) is only 800k.
		# It is always these idiots with massive Data::Dumper
		# structs or huge RecDescent parser.
		PPI::Exception::ParserRejection->throw("File is too large");

	} elsif ( $self->{source_bytes} ) {
		# Split on local newlines
		$self->{source} =~ s/(?:\015{1,2}\012|\015|\012)/\n/g;
		$self->{source} = [ split /(?<=\n)/, $self->{source} ];

	} else {
		$self->{source} = [ ];
	}

	### EVIL
	# I'm explaining this earlier than I should so you can understand
	# why I'm about to do something that looks very strange. There's
	# a problem with the Tokenizer, in that tokens tend to change
	# classes as each letter is added, but they don't get allocated
	# their definite final class until the "end" of the token, the
	# detection of which occurs in about a hundred different places,
	# all through various crufty code (that triples the speed).
	#
	# However, in general, this does not apply to tokens in which a
	# whitespace character is valid, such as comments, whitespace and
	# big strings.
	#
	# So what we do is add a space to the end of the source. This
	# triggers normal "end of token" functionality for all cases. Then,
	# once the tokenizer hits end of file, it examines the last token to
	# manually either remove the ' ' token, or chop it off the end of
	# a longer one in which the space would be valid.
	if ( List::MoreUtils::any { /^__(?:DATA|END)__\s*$/ } @{$self->{source}} ) {
		$self->{source_eof_chop} = '';
	} elsif ( ! defined $self->{source}->[0] ) {
		$self->{source_eof_chop} = '';
	} elsif ( $self->{source}->[-1] =~ /\s$/ ) {
		$self->{source_eof_chop} = '';
	} else {
		$self->{source_eof_chop} = 1;
		$self->{source}->[-1] .= ' ';
	}

	$self;
}





#####################################################################
# Main Public Methods

=pod

=head2 get_token

When using the PPI::Tokenizer object as an iterator, the C<get_token>
method is the primary method that is used. It increments the cursor
and returns the next Token in the output array.

The actual parsing of the file is done only as-needed, and a line at
a time. When C<get_token> hits the end of the token array, it will
cause the parser to pull in the next line and parse it, continuing
as needed until there are more tokens on the output array that
get_token can then return.

This means that a number of Tokenizer objects can be created, and
won't consume significant CPU until you actually begin to pull tokens
from it.

Return a L<PPI::Token> object on success, C<0> if the Tokenizer had
reached the end of the file, or C<undef> on error.

=cut

sub get_token {
	my $self = shift;

	# Shortcut for EOF
	if ( $self->{token_eof}
	 and $self->{token_cursor} > scalar @{$self->{tokens}}
	) {
		return 0;
	}

	# Return the next token if we can
	if ( my $token = $self->{tokens}->[ $self->{token_cursor} ] ) {
		$self->{token_cursor}++;
		return $token;
	}

	my $line_rv;

	# Catch exceptions and return undef, so that we
	# can start to convert code to exception-based code.
	my $rv = eval {
		# No token, we need to get some more
		while ( $line_rv = $self->_process_next_line ) {
			# If there is something in the buffer, return it
			# The defined() prevents a ton of calls to PPI::Util::TRUE
			if ( defined( my $token = $self->{tokens}->[ $self->{token_cursor} ] ) ) {
				$self->{token_cursor}++;
				return $token;
			}
		}
		return undef;
	};
	if ( $@ ) {
		if ( _INSTANCE($@, 'PPI::Exception') ) {
			$@->throw;
		} else {
			my $errstr = $@;
			$errstr =~ s/^(.*) at line .+$/$1/;
			PPI::Exception->throw( $errstr );
		}
	} elsif ( $rv ) {
		return $rv;
	}

	if ( defined $line_rv ) {
		# End of file, but we can still return things from the buffer
		if ( my $token = $self->{tokens}->[ $self->{token_cursor} ] ) {
			$self->{token_cursor}++;
			return $token;
		}

		# Set our token end of file flag
		$self->{token_eof} = 1;
		return 0;
	}

	# Error, pass it up to our caller
	undef;
}

=pod

=head2 all_tokens

When not being used as an iterator, the C<all_tokens> method tells
the Tokenizer to parse the entire file and return all of the tokens
in a single ARRAY reference.

It should be noted that C<all_tokens> does B<NOT> interfere with the
use of the Tokenizer object as an iterator (does not modify the token
cursor) and use of the two different mechanisms can be mixed safely.

Returns a reference to an ARRAY of L<PPI::Token> objects on success
or throws an exception on error.

=cut

sub all_tokens {
	my $self = shift;

	# Catch exceptions and return undef, so that we
	# can start to convert code to exception-based code.
	eval {
		# Process lines until we get EOF
		unless ( $self->{token_eof} ) {
			my $rv;
			while ( $rv = $self->_process_next_line ) {}
			unless ( defined $rv ) {
				PPI::Exception->throw("Error while processing source");
			}

			# Clean up the end of the tokenizer
			$self->_clean_eof;
		}
	};
	if ( $@ ) {
		my $errstr = $@;
		$errstr =~ s/^(.*) at line .+$/$1/;
		PPI::Exception->throw( $errstr );
	}

	# End of file, return a copy of the token array.
	return [ @{$self->{tokens}} ];
}

=pod

=head2 increment_cursor

Although exposed as a public method, C<increment_method> is implemented
for expert use only, when writing lexers or other components that work
directly on token streams.

It manually increments the token cursor forward through the file, in effect
"skipping" the next token.

Return true if the cursor is incremented, C<0> if already at the end of
the file, or C<undef> on error.

=cut

sub increment_cursor {
	# Do this via the get_token method, which makes sure there
	# is actually a token there to move to.
	$_[0]->get_token and 1;
}

=pod

=head2 decrement_cursor

Although exposed as a public method, C<decrement_method> is implemented
for expert use only, when writing lexers or other components that work
directly on token streams.

It manually decrements the token cursor backwards through the file, in
effect "rolling back" the token stream. And indeed that is what it is
primarily intended for, when the component that is consuming the token
stream needs to implement some sort of "roll back" feature in its use
of the token stream.

Return true if the cursor is decremented, C<0> if already at the
beginning of the file, or C<undef> on error.

=cut

sub decrement_cursor {
	my $self = shift;

	# Check for the beginning of the file
	return 0 unless $self->{token_cursor};

	# Decrement the token cursor
	$self->{token_eof} = 0;
	--$self->{token_cursor};
}





#####################################################################
# Working With Source

# Fetches the next line from the input line buffer
# Returns undef at EOF.
sub _get_line {
	my $self = shift;
	return undef unless $self->{source}; # EOF hit previously

	# Pull off the next line
	my $line = shift @{$self->{source}};

	# Flag EOF if we hit it
	$self->{source} = undef unless defined $line;

	# Return the line (or EOF flag)
	return $line; # string or undef
}

# Fetches the next line, ready to process
# Returns 1 on success
# Returns 0 on EOF
sub _fill_line {
	my $self   = shift;
	my $inscan = shift;

	# Get the next line
	my $line = $self->_get_line;
	unless ( defined $line ) {
		# End of file
		unless ( $inscan ) {
			delete $self->{line};
			delete $self->{line_cursor};
			delete $self->{line_length};
			return 0;
		}

		# In the scan version, just set the cursor to the end
		# of the line, and the rest should just cascade out.
		$self->{line_cursor} = $self->{line_length};
		return 0;
	}

	# Populate the appropriate variables
	$self->{line}        = $line;
	$self->{line_cursor} = -1;
	$self->{line_length} = length $line;
	$self->{line_count}++;

	1;
}

# Get the current character
sub _char {
	my $self = shift;
	substr( $self->{line}, $self->{line_cursor}, 1 );
}





####################################################################
# Per line processing methods

# Processes the next line
# Returns 1 on success completion
# Returns 0 if EOF
# Returns undef on error
sub _process_next_line {
	my $self = shift;

	# Fill the line buffer
	my $rv;
	unless ( $rv = $self->_fill_line ) {
		return undef unless defined $rv;

		# End of file, finalize last token
		$self->_finalize_token;
		return 0;
	}

	# Run the __TOKENIZER__on_line_start
	$rv = $self->{class}->__TOKENIZER__on_line_start( $self );
	unless ( $rv ) {
		# If there are no more source lines, then clean up
		if ( ref $self->{source} eq 'ARRAY' and ! @{$self->{source}} ) {
			$self->_clean_eof;
		}

		# Defined but false means next line
		return 1 if defined $rv;
		PPI::Exception->throw("Error at line $self->{line_count}");
	}

	# If we can't deal with the entire line, process char by char
	while ( $rv = $self->_process_next_char ) {}
	unless ( defined $rv ) {
		PPI::Exception->throw("Error at line $self->{line_count}, character $self->{line_cursor}");
	}

	# Trigger any action that needs to happen at the end of a line
	$self->{class}->__TOKENIZER__on_line_end( $self );

	# If there are no more source lines, then clean up
	unless ( ref($self->{source}) eq 'ARRAY' and @{$self->{source}} ) {
		return $self->_clean_eof;
	}

	return 1;
}





#####################################################################
# Per-character processing methods

# Process on a per-character basis.
# Note that due the the high number of times this gets
# called, it has been fairly heavily in-lined, so the code
# might look a bit ugly and duplicated.
sub _process_next_char {
	my $self = shift;

	### FIXME - This checks for a screwed up condition that triggers
	###         several warnings, amoungst other things.
	if ( ! defined $self->{line_cursor} or ! defined $self->{line_length} ) {
		# $DB::single = 1;
		return undef;
	}

	# Increment the counter and check for end of line
	return 0 if ++$self->{line_cursor} >= $self->{line_length};

	# Pass control to the token class
        my $result;
	unless ( $result = $self->{class}->__TOKENIZER__on_char( $self ) ) {
		# undef is error. 0 is "Did stuff ourself, you don't have to do anything"
		return defined $result ? 1 : undef;
	}

	# We will need the value of the current character
	my $char = substr( $self->{line}, $self->{line_cursor}, 1 );
	if ( $result eq '1' ) {
		# If __TOKENIZER__on_char returns 1, it is signaling that it thinks that
		# the character is part of it.

		# Add the character
		if ( defined $self->{token} ) {
			$self->{token}->{content} .= $char;
		} else {
			defined($self->{token} = $self->{class}->new($char)) or return undef;
		}

		return 1;
	}

	# We have been provided with the name of a class
	if ( $self->{class} ne "PPI::Token::$result" ) {
		# New class
		$self->_new_token( $result, $char );
	} elsif ( defined $self->{token} ) {
		# Same class as current
		$self->{token}->{content} .= $char;
	} else {
		# Same class, but no current
		defined($self->{token} = $self->{class}->new($char)) or return undef;
	}

	1;
}





#####################################################################
# Altering Tokens in Tokenizer

# Finish the end of a token.
# Returns the resulting parse class as a convenience.
sub _finalize_token {
	my $self = shift;
	return $self->{class} unless defined $self->{token};

	# Add the token to the token buffer
	push @{ $self->{tokens} }, $self->{token};
	$self->{token} = undef;

	# Return the parse class to that of the zone we are in
	$self->{class} = $self->{zone};
}

# Creates a new token and sets it in the tokenizer
# The defined() in here prevent a ton of calls to PPI::Util::TRUE
sub _new_token {
	my $self = shift;
	# throw PPI::Exception() unless @_;
	my $class = substr( $_[0], 0, 12 ) eq 'PPI::Token::'
		? shift : 'PPI::Token::' . shift;

	# Finalize any existing token
	$self->_finalize_token if defined $self->{token};

	# Create the new token and update the parse class
	defined($self->{token} = $class->new($_[0])) or PPI::Exception->throw;
	$self->{class} = $class;

	1;
}

# At the end of the file, we need to clean up the results of the erroneous
# space that we inserted at the beginning of the process.
sub _clean_eof {
	my $self = shift;

	# Finish any partially completed token
	$self->_finalize_token if $self->{token};

	# Find the last token, and if it has no content, kill it.
	# There appears to be some evidence that such "null tokens" are
	# somehow getting created accidentally.
	my $last_token = $self->{tokens}->[ -1 ];
	unless ( length $last_token->{content} ) {
		pop @{$self->{tokens}};
	}

	# Now, if the last character of the last token is a space we added,
	# chop it off, deleting the token if there's nothing else left.
	if ( $self->{source_eof_chop} ) {
		$last_token = $self->{tokens}->[ -1 ];
		$last_token->{content} =~ s/ $//;
		unless ( length $last_token->{content} ) {
			# Popping token
			pop @{$self->{tokens}};
		}

		# The hack involving adding an extra space is now reversed, and
		# now nobody will ever know. The perfect crime!
		$self->{source_eof_chop} = '';
	}

	1;
}





#####################################################################
# Utility Methods

# Context
sub _last_token {
	$_[0]->{tokens}->[-1];
}

sub _last_significant_token {
	my $self   = shift;
	my $cursor = $#{ $self->{tokens} };
	while ( $cursor >= 0 ) {
		my $token = $self->{tokens}->[$cursor--];
		return $token if $token->significant;
	}

	# Nothing...
	PPI::Token::Whitespace->null;
}

# Get an array ref of previous significant tokens.
# Like _last_significant_token except it gets more than just one token
# Returns array ref on success.
# Returns 0 on not enough tokens
sub _previous_significant_tokens {
	my $self   = shift;
	my $count  = shift || 1;
	my $cursor = $#{ $self->{tokens} };

	my ($token, @tokens);
	while ( $cursor >= 0 ) {
		$token = $self->{tokens}->[$cursor--];
		if ( $token->significant ) {
			push @tokens, $token;
			return \@tokens if scalar @tokens >= $count;
		}
	}

	# Pad with empties
	foreach ( 1 .. ($count - scalar @tokens) ) {
		push @tokens, PPI::Token::Whitespace->null;
	}

	\@tokens;
}

my %OBVIOUS_CLASS = (
	'PPI::Token::Symbol'              => 'operator',
	'PPI::Token::Magic'               => 'operator',
	'PPI::Token::Number'              => 'operator',
	'PPI::Token::ArrayIndex'          => 'operator',
	'PPI::Token::Quote::Double'       => 'operator',
	'PPI::Token::Quote::Interpolate'  => 'operator',
	'PPI::Token::Quote::Literal'      => 'operator',
	'PPI::Token::Quote::Single'       => 'operator',
	'PPI::Token::QuoteLike::Backtick' => 'operator',
	'PPI::Token::QuoteLike::Command'  => 'operator',
	'PPI::Token::QuoteLike::Readline' => 'operator',
	'PPI::Token::QuoteLike::Regexp'   => 'operator',
	'PPI::Token::QuoteLike::Words'    => 'operator',
);

my %OBVIOUS_CONTENT = (
	'(' => 'operand',
	'{' => 'operand',
	'[' => 'operand',
	';' => 'operand',
	'}' => 'operator',
);

# Try to determine operator/operand context, is possible.
# Returns "operator", "operand", or "" if unknown.
sub _opcontext {
	my $self   = shift;
	my $tokens = $self->_previous_significant_tokens(1);
	my $p0     = $tokens->[0];
	my $c0     = ref $p0;

	# Map the obvious cases
	return $OBVIOUS_CLASS{$c0}   if defined $OBVIOUS_CLASS{$c0};
	return $OBVIOUS_CONTENT{$p0} if defined $OBVIOUS_CONTENT{$p0};

	# Most of the time after an operator, we are an operand
	return 'operand' if $p0->isa('PPI::Token::Operator');

	# If there's NOTHING, it's operand
	return 'operand' if $p0->content eq '';

	# Otherwise, we don't know
	return ''
}

1;

=pod

=head1 NOTES

=head2 How the Tokenizer Works

Understanding the Tokenizer is not for the feint-hearted. It is by far
the most complex and twisty piece of perl I've ever written that is actually
still built properly and isn't a terrible spaghetti-like mess. In fact, you
probably want to skip this section.

But if you really want to understand, well then here goes.

=head2 Source Input and Clean Up

The Tokenizer starts by taking source in a variety of forms, sucking it
all in and merging into one big string, and doing our own internal line
split, using a "universal line separator" which allows the Tokenizer to
take source for any platform (and even supports a few known types of
broken newlines caused by mixed mac/pc/*nix editor screw ups).

The resulting array of lines is used to feed the tokenizer, and is also
accessed directly by the heredoc-logic to do the line-oriented part of
here-doc support.

=head2 Doing Things the Old Fashioned Way

Due to the complexity of perl, and after 2 previously aborted parser
attempts, in the end the tokenizer was fashioned around a line-buffered
character-by-character method.

That is, the Tokenizer pulls and holds a line at a time into a line buffer,
and then iterates a cursor along it. At each cursor position, a method is
called in whatever token class we are currently in, which will examine the
character at the current position, and handle it.

As the handler methods in the various token classes are called, they
build up a output token array for the source code.

Various parts of the Tokenizer use look-ahead, arbitrary-distance
look-behind (although currently the maximum is three significant tokens),
or both, and various other heuristic guesses.

I've been told it is officially termed a I<"backtracking parser
with infinite lookaheads">.

=head2 State Variables

Aside from the current line and the character cursor, the Tokenizer
maintains a number of different state variables.

=over

=item Current Class

The Tokenizer maintains the current token class at all times. Much of the
time is just going to be the "Whitespace" class, which is what the base of
a document is. As the tokenizer executes the various character handlers,
the class changes a lot as it moves a long. In fact, in some instances,
the character handler may not handle the character directly itself, but
rather change the "current class" and then hand off to the character
handler for the new class.

Because of this, and some other things I'll deal with later, the number of
times the character handlers are called does not in fact have a direct
relationship to the number of actual characters in the document.

=item Current Zone

Rather than create a class stack to allow for infinitely nested layers of
classes, the Tokenizer recognises just a single layer.

To put it a different way, in various parts of the file, the Tokenizer will
recognise different "base" or "substrate" classes. When a Token such as a
comment or a number is finalised by the tokenizer, it "falls back" to the
base state.

This allows proper tokenization of special areas such as __DATA__
and __END__ blocks, which also contain things like comments and POD,
without allowing the creation of any significant Tokens inside these areas.

For the main part of a document we use L<PPI::Token::Whitespace> for this,
with the idea being that code is "floating in a sea of whitespace".

=item Current Token

The final main state variable is the "current token". This is the Token
that is currently being built by the Tokenizer. For certain types, it
can be manipulated and morphed and change class quite a bit while being
assembled, as the Tokenizer's understanding of the token content changes.

When the Tokenizer is confident that it has seen the end of the Token, it
will be "finalized", which adds it to the output token array and resets
the current class to that of the zone that we are currently in.

I should also note at this point that the "current token" variable is
optional. The Tokenizer is capable of knowing what class it is currently
set to, without actually having accumulated any characters in the Token.

=back

=head2 Making It Faster

As I'm sure you can imagine, calling several different methods for each
character and running regexes and other complex heuristics made the first
fully working version of the tokenizer extremely slow.

During testing, I created a metric to measure parsing speed called
LPGC, or "lines per gigacycle" . A gigacycle is simple a billion CPU
cycles on a typical single-core CPU, and so a Tokenizer running at
"1000 lines per gigacycle" should generate around 1200 lines of tokenized
code when running on a 1200 MHz processor.

The first working version of the tokenizer ran at only 350 LPGC, so to
tokenize a typical large module such as L<ExtUtils::MakeMaker> took
10-15 seconds. This sluggishness made it unpractical for many uses.

So in the current parser, there are multiple layers of optimisation
very carefully built in to the basic. This has brought the tokenizer
up to a more reasonable 1000 LPGC, at the expense of making the code
quite a bit twistier.

=head2 Making It Faster - Whole Line Classification

The first step in the optimisation process was to add a hew handler to
enable several of the more basic classes (whitespace, comments) to be
able to be parsed a line at a time. At the start of each line, a
special optional handler (only supported by a few classes) is called to
check and see if the entire line can be parsed in one go.

This is used mainly to handle things like POD, comments, empty lines,
and a few other minor special cases.

=head2 Making It Faster - Inlining

The second stage of the optimisation involved inlining a small
number of critical methods that were repeated an extremely high number
of times. Profiling suggested that there were about 1,000,000 individual
method calls per gigacycle, and by cutting these by two thirds a significant
speed improvement was gained, in the order of about 50%.

You may notice that many methods in the C<PPI::Tokenizer> code look
very nested and long hand. This is primarily due to this inlining.

At around this time, some statistics code that existed in the early
versions of the parser was also removed, as it was determined that
it was consuming around 15% of the CPU for the entire parser, while
making the core more complicated.

A judgment call was made that with the difficulties likely to be
encountered with future planned enhancements, and given the relatively
high cost involved, the statistics features would be removed from the
Tokenizer.

=head2 Making It Faster - Quote Engine

Once inlining had reached diminishing returns, it became obvious from
the profiling results that a huge amount of time was being spent
stepping a char at a time though long, simple and "syntactically boring"
code such as comments and strings.

The existing regex engine was expanded to also encompass quotes and
other quote-like things, and a special abstract base class was added
that provided a number of specialised parsing methods that would "scan
ahead", looking out ahead to find the end of a string, and updating
the cursor to leave it in a valid position for the next call.

This is also the point at which the number of character handler calls began
to greatly differ from the number of characters. But it has been done
in a way that allows the parser to retain the power of the original
version at the critical points, while skipping through the "boring bits"
as needed for additional speed.

The addition of this feature allowed the tokenizer to exceed 1000 LPGC
for the first time.

=head2 Making It Faster - The "Complete" Mechanism

As it became evident that great speed increases were available by using
this "skipping ahead" mechanism, a new handler method was added that
explicitly handles the parsing of an entire token, where the structure
of the token is relatively simple. Tokens such as symbols fit this case,
as once we are passed the initial sigil and word char, we know that we
can skip ahead and "complete" the rest of the token much more easily.

A number of these have been added for most or possibly all of the common
cases, with most of these "complete" handlers implemented using regular
expressions.

In fact, so many have been added that at this point, you could arguably
reclassify the tokenizer as a "hybrid regex, char-by=char heuristic
tokenizer". More tokens are now consumed in "complete" methods in a
typical program than are handled by the normal char-by-char methods.

Many of the these complete-handlers were implemented during the writing
of the Lexer, and this has allowed the full parser to maintain around
1000 LPGC despite the increasing weight of the Lexer.

=head2 Making It Faster - Porting To C (In Progress)

While it would be extraordinarily difficult to port all of the Tokenizer
to C, work has started on a L<PPI::XS> "accelerator" package which acts as
a separate and automatically-detected add-on to the main PPI package.

L<PPI::XS> implements faster versions of a variety of functions scattered
over the entire PPI codebase, from the Tokenizer Core, Quote Engine, and
various other places, and implements them identically in XS/C.

In particular, the skip-ahead methods from the Quote Engine would appear
to be extremely amenable to being done in C, and a number of other
functions could be cherry-picked one at a time and implemented in C.

Each method is heavily tested to ensure that the functionality is
identical, and a versioning mechanism is included to ensure that if a
function gets out of sync, L<PPI::XS> will degrade gracefully and just
not replace that single method.

=head1 TO DO

- Add an option to reset or seek the token stream...

- Implement more Tokenizer functions in L<PPI::XS>

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
