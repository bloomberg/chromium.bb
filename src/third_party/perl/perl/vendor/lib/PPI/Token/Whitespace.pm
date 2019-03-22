package PPI::Token::Whitespace;

=pod

=head1 NAME

PPI::Token::Whitespace - Tokens representing ordinary white space

=head1 INHERITANCE

  PPI::Token::Whitespace
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

As a full "round-trip" parser, PPI records every last byte in a
file and ensure that it is included in the L<PPI::Document> object.

This even includes whitespace. In fact, Perl documents are seen
as "floating in a sea of whitespace", and thus any document will
contain vast quantities of C<PPI::Token::Whitespace> objects.

For the most part, you shouldn't notice them. Or at least, you
shouldn't B<have> to notice them.

This means doing things like consistently using the "S for significant"
series of L<PPI::Node> and L<PPI::Element> methods to do things.

If you want the nth child element, you should be using C<schild> rather
than C<child>, and likewise C<snext_sibling>, C<sprevious_sibling>, and
so on and so forth.

=head1 METHODS

Again, for the most part you should really B<not> need to do anything
very significant with whitespace.

But there are a couple of convenience methods provided, beyond those
provided by the parent L<PPI::Token> and L<PPI::Element> classes.

=cut

use strict;
use Clone      ();
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}

=pod

=head2 null

Because L<PPI> sees documents as sitting on a sort of substrate made of
whitespace, there are a couple of corner cases that get particularly
nasty if they don't find whitespace in certain places.

Imagine walking down the beach to go into the ocean, and then quite
unexpectedly falling off the side of the planet. Well it's somewhat
equivalent to that, including the whole screaming death bit.

The C<null> method is a convenience provided to get some internals
out of some of these corner cases.

Specifically it create a whitespace token that represents nothing,
or at least the null string C<''>. It's a handy way to have some
"whitespace" right where you need it, without having to have any
actual characters.

=cut

my $null = undef;

sub null {
	$null ||= $_[0]->new('');
	Clone::clone($null);
}

### XS -> PPI/XS.xs:_PPI_Token_Whitespace__significant 0.900+
sub significant { '' }

=pod

=head2 tidy

C<tidy> is a convenience method for removing unneeded whitespace.

Specifically, it removes any whitespace from the end of a line.

Note that this B<doesn't> include POD, where you may well need
to keep certain types of whitespace. The entire POD chunk lives
in its own L<PPI::Token::Pod> object.

=cut

sub tidy {
	$_[0]->{content} =~ s/^\s+?(?>\n)//;
	1;
}





#####################################################################
# Parsing Methods

# Build the class and commit maps
use vars qw{ @CLASSMAP @COMMITMAP %MATCHWORD };
BEGIN {
	@CLASSMAP  = ();
	@COMMITMAP = ();
	foreach (
		'a' .. 'u', 'w', 'y', 'z', 'A' .. 'Z', '_'
	) {
		$COMMITMAP[ord $_] = 'PPI::Token::Word';
	}
	foreach ( qw!; [ ] { } )! )       { $COMMITMAP[ord $_] = 'PPI::Token::Structure' }
	foreach ( 0 .. 9 )                { $CLASSMAP[ord $_]  = 'Number'   }
	foreach ( qw{= ? | + > . ! ~ ^} ) { $CLASSMAP[ord $_]  = 'Operator' }
	foreach ( qw{* $ @ & : %} )       { $CLASSMAP[ord $_]  = 'Unknown'  }

	# Miscellaneous remainder
	$COMMITMAP[ord '#'] = 'PPI::Token::Comment';
	$COMMITMAP[ord 'v'] = 'PPI::Token::Number::Version';
	$CLASSMAP[ord ',']  = 'PPI::Token::Operator';
	$CLASSMAP[ord "'"]  = 'Quote::Single';
	$CLASSMAP[ord '"']  = 'Quote::Double';
	$CLASSMAP[ord '`']  = 'QuoteLike::Backtick';
	$CLASSMAP[ord '\\'] = 'Cast';
	$CLASSMAP[ord '_']  = 'Word';
	$CLASSMAP[9]        = 'Whitespace'; # A horizontal tab
	$CLASSMAP[10]       = 'Whitespace'; # A newline
	$CLASSMAP[13]       = 'Whitespace'; # A carriage return
	$CLASSMAP[32]       = 'Whitespace'; # A normal space

	# Words (functions and keywords) after which a following / is
	# almost certainly going to be a regex
	%MATCHWORD = map { $_ => 1 } qw{
		split
		if
		unless
		grep
		map
	};
}

sub __TOKENIZER__on_line_start {
	my $t    = $_[1];
	my $line = $t->{line};

	# Can we classify the entire line in one go
	if ( $line =~ /^\s*$/ ) {
		# A whitespace line
		$t->_new_token( 'Whitespace', $line );
		return 0;

	} elsif ( $line =~ /^\s*#/ ) {
		# A comment line
		$t->_new_token( 'Comment', $line );
		$t->_finalize_token;
		return 0;

	} elsif ( $line =~ /^=(\w+)/ ) {
		# A Pod tag... change to pod mode
		$t->_new_token( 'Pod', $line );
		if ( $1 eq 'cut' ) {
			# This is an error, but one we'll ignore
			# Don't go into Pod mode, since =cut normally
			# signals the end of Pod mode
		} else {
			$t->{class} = 'PPI::Token::Pod';
		}
		return 0;

	} elsif ( $line =~ /^use v6\-alpha\;/ ) {
		# Indicates a Perl 6 block. Make the initial
		# implementation just suck in the entire rest of the
		# file.
		my @perl6 = ();
		while ( 1 ) {
			my $line6 = $t->_get_line;
			last unless defined $line6;
			push @perl6, $line6;
		}
		push @{ $t->{perl6} }, join '', @perl6;

		# We only sucked in the block, we don't actially do
		# anything to the "use v6..." line. So return as if
		# we didn't find anything at all.
		return 1;
	}

	1;
}

sub __TOKENIZER__on_char {
	my $t    = $_[1];
	my $char = ord substr $t->{line}, $t->{line_cursor}, 1;

	# Do we definately know what something is?
	return $COMMITMAP[$char]->__TOKENIZER__commit($t) if $COMMITMAP[$char];

	# Handle the simple option first
	return $CLASSMAP[$char] if $CLASSMAP[$char];

	if ( $char == 40 ) {  # $char eq '('
		# Finalise any whitespace token...
		$t->_finalize_token if $t->{token};

		# Is this the beginning of a sub prototype?
		# We are a sub prototype IF
		# 1. The previous significant token is a bareword.
		# 2. The one before that is the word 'sub'.
		# 3. The one before that is a 'structure'

		# Get the three previous significant tokens
		my $tokens = $t->_previous_significant_tokens(3);
		if ( $tokens ) {
			# A normal subroutine declaration
			my $p1 = $tokens->[1];
			my $p2 = $tokens->[2];
			if (
				$tokens->[0]->isa('PPI::Token::Word')
				and
				$p1->isa('PPI::Token::Word')
				and
				$p1->content eq 'sub'
				and (
					$p2->isa('PPI::Token::Structure')
					or (
						$p2->isa('PPI::Token::Whitespace')
						and
						$p2->content eq ''
					)
				)
			) {
				# This is a sub prototype
				return 'Prototype';
			}

			# An prototyped anonymous subroutine
			my $p0 = $tokens->[0];
			if ( $p0->isa('PPI::Token::Word') and $p0->content eq 'sub'
				# Maybe it's invoking a method named 'sub'
				and not ( $p1 and $p1->isa('PPI::Token::Operator') and $p1->content eq '->')
			) {
				return 'Prototype';
			}
		}

		# This is a normal open bracket
		return 'Structure';

	} elsif ( $char == 60 ) { # $char eq '<'
		# Finalise any whitespace token...
		$t->_finalize_token if $t->{token};

		# This is either "less than" or "readline quote-like"
		# Do some context stuff to guess which.
		my $prev = $t->_last_significant_token;

		# The most common group of less-thans are used like
		# $foo < $bar
		# 1 < $bar
		# $#foo < $bar
		return 'Operator' if $prev->isa('PPI::Token::Symbol');
		return 'Operator' if $prev->isa('PPI::Token::Magic');
		return 'Operator' if $prev->isa('PPI::Token::Number');
		return 'Operator' if $prev->isa('PPI::Token::ArrayIndex');

		# If it is <<... it's a here-doc instead
		my $next_char = substr( $t->{line}, $t->{line_cursor} + 1, 1 );
		if ( $next_char eq '<' ) {
			return 'Operator';
		}

		# The most common group of readlines are used like
		# while ( <...> )
		# while <>;
		my $prec = $prev->content;
		if ( $prev->isa('PPI::Token::Structure') and $prec eq '(' ) {
			return 'QuoteLike::Readline';
		}
		if ( $prev->isa('PPI::Token::Word') and $prec eq 'while' ) {
			return 'QuoteLike::Readline';
		}
		if ( $prev->isa('PPI::Token::Operator') and $prec eq '=' ) {
			return 'QuoteLike::Readline';
		}
		if ( $prev->isa('PPI::Token::Operator') and $prec eq ',' ) {
			return 'QuoteLike::Readline';
		}

		if ( $prev->isa('PPI::Token::Structure') and $prec eq '}' ) {
			# Could go either way... do a regex check
			# $foo->{bar} < 2;
			# grep { .. } <foo>;
			my $line = substr( $t->{line}, $t->{line_cursor} );
			if ( $line =~ /^<(?!\d)\w+>/ ) {
				# Almost definitely readline
				return 'QuoteLike::Readline';
			}
		}

		# Otherwise, we guess operator, which has been the default up
		# until this more comprehensive section was created.
		return 'Operator';

	} elsif ( $char == 47 ) { #  $char eq '/'
		# Finalise any whitespace token...
		$t->_finalize_token if $t->{token};

		# This is either a "divided by" or a "start regex"
		# Do some context stuff to guess ( ack ) which.
		# Hopefully the guess will be good enough.
		my $prev = $t->_last_significant_token;
		my $prec = $prev->content;

		# Most times following an operator, we are a regex.
		# This includes cases such as:
		# ,  - As an argument in a list 
		# .. - The second condition in a flip flop
		# =~ - A bound regex
		# !~ - Ditto
		return 'Regexp::Match' if $prev->isa('PPI::Token::Operator');

		# After a symbol
		return 'Operator' if $prev->isa('PPI::Token::Symbol');
		if ( $prec eq ']' and $prev->isa('PPI::Token::Structure') ) {
			return 'Operator';
		}

		# After another number
		return 'Operator' if $prev->isa('PPI::Token::Number');

		# After going into scope/brackets
		if (
			$prev->isa('PPI::Token::Structure')
			and (
				$prec eq '('
				or
				$prec eq '{'
				or
				$prec eq ';'
			)
		) {
			return 'Regexp::Match';
		}

		# Functions and keywords
		if (
			$MATCHWORD{$prec}
			and
			$prev->isa('PPI::Token::Word')
		) {
			return 'Regexp::Match';
		}

		# Or as the very first thing in a file
		return 'Regexp::Match' if $prec eq '';

		# What about the char after the slash? There's some things
		# that would be highly illogical to see if its an operator.
		my $next_char = substr $t->{line}, $t->{line_cursor} + 1, 1;
		if ( defined $next_char and length $next_char ) {
			if ( $next_char =~ /(?:\^|\[|\\)/ ) {
				return 'Regexp::Match';
			}
		}

		# Otherwise... erm... assume operator?
		# Add more tests here as potential cases come to light
		return 'Operator';

	} elsif ( $char == 120 ) { # $char eq 'x'
		# Handle an arcane special case where "string"x10 means the x is an operator.
		# String in this case means ::Single, ::Double or ::Execute, or the operator versions or same.
		my $nextchar = substr $t->{line}, $t->{line_cursor} + 1, 1;
		my $prev     = $t->_previous_significant_tokens(1);
		$prev = ref $prev->[0];
		if ( $nextchar =~ /\d/ and $prev ) {
			if ( $prev =~ /::Quote::(?:Operator)?(?:Single|Double|Execute)$/ ) {
				return 'Operator';
			}
		}

		# Otherwise, commit like a normal bareword
		return PPI::Token::Word->__TOKENIZER__commit($t);

	} elsif ( $char == 45 ) { # $char eq '-'
		# Look for an obvious operator operand context
		my $context = $t->_opcontext;
		if ( $context eq 'operator' ) {
			return 'Operator';
		} else {
			# More logic needed
			return 'Unknown';
		}

	} elsif ( $char >= 128 ) { # Outside ASCII
		return 'PPI::Token::Word'->__TOKENIZER__commit($t) if $t =~ /\w/;
		return 'Whitespace' if $t =~ /\s/;
        }


	# All the whitespaces are covered, so what to do
	### For now, die
	PPI::Exception->throw("Encountered unexpected character '$char'");
}

sub __TOKENIZER__on_line_end {
	$_[1]->_finalize_token if $_[1]->{token};
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
