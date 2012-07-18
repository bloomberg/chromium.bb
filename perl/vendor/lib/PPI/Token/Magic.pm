package PPI::Token::Magic;

=pod

=head1 NAME

PPI::Token::Magic - Tokens representing magic variables

=head1 INHERITANCE

  PPI::Token::Magic
  isa PPI::Token::Symbol
      isa PPI::Token
          isa PPI::Element

=head1 SYNOPSIS

  # When we say magic variables, we mean these...
  $1   $2   $3   $4   $5   $6   $7   $8   $9
  $_   $&   $`   $'   $+   @+   %+   $*   $.    $/    $|
  $\\  $"   $;   $%   $=   $-   @-   %-   $)    $#
  $~   $^   $:   $?   $!   %!   $@   $$   $<    $>
  $(   $0   $[   $]   @_   @*   $}   $,   $#+   $#-
  $^L  $^A  $^E  $^C  $^D  $^F  $^H
  $^I  $^M  $^N  $^O  $^P  $^R  $^S
  $^T  $^V  $^W  $^X

=head1 DESCRIPTION

C<PPI::Token::Magic> is a sub-class of L<PPI::Token::Symbol> which
identifies the token as "magic variable", one of the strange and
unusual variables that are connected to "things" behind the scenes.

Some are extremely common, like C<$_>, and others you will quite
probably never encounter in your Perl career.

=head1 METHODS

The class provides no additional methods, beyond those provided by it's
L<PPI::Token::Symbol>, L<PPI::Token> and L<PPI::Element>.

=cut

use strict;
use PPI::Token::Symbol ();
use PPI::Token::Unknown ();

use vars qw{$VERSION @ISA %magic};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token::Symbol';

	# Magic variables taken from perlvar.
	# Several things added separately to avoid warnings.
	foreach ( qw{
		$1 $2 $3 $4 $5 $6 $7 $8 $9
		$_ $& $` $' $+ @+ %+ $* $. $/ $|
		$\\ $" $; $% $= $- @- %- $)
		$~ $^ $: $? $! %! $@ $$ $< $>
		$( $0 $[ $] @_ @*

		$^L $^A $^E $^C $^D $^F $^H
		$^I $^M $^N $^O $^P $^R $^S
		$^T $^V $^W $^X %^H

		$::|
	}, '$}', '$,', '$#', '$#+', '$#-' ) {
		$magic{$_} = 1;
	}
}

=pod

=begin testing __TOKENIZER_on_char 30

my $document = PPI::Document->new(\<<'END_PERL');
$[;			# Magic  $[
$$;			# Magic  $$
%-;			# Magic  %-
$#-;			# Magic  $#-
$$foo;			# Symbol $foo		Dereference of $foo
$^W;			# Magic  $^W
$^WIDE_SYSTEM_CALLS;	# Magic  $^WIDE_SYSTEM_CALLS
${^MATCH};		# Magic  ${^MATCH}
@{^_Bar};		# Magic  @{^_Bar}
${^_Bar}[0];		# Magic  @{^_Bar}
%{^_Baz};		# Magic  %{^_Baz}
${^_Baz}{burfle};	# Magic  %{^_Baz}
$${^MATCH};		# Magic  ${^MATCH}	Dereference of ${^MATCH}
\${^MATCH};		# Magic  ${^MATCH}
END_PERL

isa_ok( $document, 'PPI::Document' );

$document->index_locations();

my $symbols = $document->find( 'PPI::Token::Symbol' );

is( scalar(@$symbols), 14, 'Found 14 symbols' );
my $comments = $document->find( 'PPI::Token::Comment' );

foreach my $token ( @$symbols ) {
	my ($hash, $class, $name, $remk) =
		split '\s+', $comments->[$token->line_number - 1], 4;
	isa_ok( $token, "PPI::Token::$class" );
	is( $token->symbol, $name, $remk || "The symbol is $name" );
}

=end testing

=cut

sub __TOKENIZER__on_char {
	my $t = $_[1];

	# $c is the candidate new content
	my $c = $t->{token}->{content} . substr( $t->{line}, $t->{line_cursor}, 1 );

	# Do a quick first test so we don't have to do more than this one.
	# All of the tests below match this one, so it should provide a
	# small speed up. This regex should be updated to match the inside
	# tests if they are changed.
	if ( $c =~ /^  \$  .*  [  \w  :  \$  \{  ]  $/x ) {

		if ( $c =~ /^(\$(?:\_[\w:]|::))/ or $c =~ /^\$\'[\w]/ ) {
			# If and only if we have $'\d, it is not a
			# symbol. (this was apparently a concious choice)
			# Note that $::0 on the other hand is legal
			if ( $c =~ /^\$\'\d$/ ) {
				# In this case, we have a magic plus a digit.
				# Save the CURRENT token, and rerun the on_char
				return $t->_finalize_token->__TOKENIZER__on_char( $t );
			}

			# A symbol in the style $_foo or $::foo or $'foo.
			# Overwrite the current token
			$t->{class} = $t->{token}->set_class('Symbol');
			return PPI::Token::Symbol->__TOKENIZER__on_char( $t );
		}

		if ( $c =~ /^\$\$\w/ ) {
			# This is really a scalar dereference. ( $$foo )
			# Add the current token as the cast...
			$t->{token} = PPI::Token::Cast->new( '$' );
			$t->_finalize_token;

			# ... and create a new token for the symbol
			return $t->_new_token( 'Symbol', '$' );
		}

		if ( $c eq '$${' ) {
			# This _might_ be a dereference of one of the
			# control-character symbols.
			my $line = substr $t->{line}, $t->{line_cursor} + 1;
			if ( $line =~ m/$PPI::Token::Unknown::CURLY_SYMBOL/ ) {
				# This is really a dereference. ( $${^_foo} )
				# Add the current token as the cast...
				$t->{token} = PPI::Token::Cast->new( '$' );
				$t->_finalize_token;

				# ... and create a new token for the symbol
				return $t->_new_token( 'Magic', '$' );
			}
		}

		if ( $c eq '$#$' or $c eq '$#{' ) {
			# This is really an index dereferencing cast, although
			# it has the same two chars as the magic variable $#.
			$t->{class} = $t->{token}->set_class('Cast');
			return $t->_finalize_token->__TOKENIZER__on_char( $t );
		}

		if ( $c =~ /^(\$\#)\w/ ) {
			# This is really an array index thingy ( $#array )
			$t->{token} = PPI::Token::ArrayIndex->new( "$1" );
			return PPI::Token::ArrayIndex->__TOKENIZER__on_char( $t );
		}

		if ( $c =~ /^\$\^\w+$/o ) {
			# It's an escaped char magic... maybe ( like $^M )
			my $next = substr( $t->{line}, $t->{line_cursor}+1, 1 ); # Peek ahead
			if ($magic{$c} && (!$next || $next !~ /\w/)) {
				$t->{token}->{content} = $c;
				$t->{line_cursor}++;
			} else {
				# Maybe it's a long magic variable like $^WIDE_SYSTEM_CALLS
				return 1;
			}
		}

		if ( $c =~ /^\$\#\{/ ) {
			# The $# is actually a case, and { is its block
			# Add the current token as the cast...
			$t->{token} = PPI::Token::Cast->new( '$#' );
			$t->_finalize_token;

			# ... and create a new token for the block
			return $t->_new_token( 'Structure', '{' );
		}
	} elsif ($c =~ /^%\^/) {
		return 1 if $c eq '%^';
		# It's an escaped char magic... maybe ( like %^H )
		if ($magic{$c}) {
			$t->{token}->{content} = $c;
			$t->{line_cursor}++;
		} else {
			# Back off, treat '%' as an operator
			chop $t->{token}->{content};
			bless $t->{token}, $t->{class} = 'PPI::Token::Operator';
			$t->{line_cursor}--;
		}
	}

	if ( $magic{$c} ) {
		# $#+ and $#-
		$t->{line_cursor} += length( $c ) - length( $t->{token}->{content} );
		$t->{token}->{content} = $c;
	} else {
		my $line = substr( $t->{line}, $t->{line_cursor} );
		if ( $line =~ /($PPI::Token::Unknown::CURLY_SYMBOL)/ ) {
			# control character symbol (e.g. ${^MATCH})
			$t->{token}->{content} .= $1;
			$t->{line_cursor}      += length $1;
		}
	}

	# End the current magic token, and recheck
	$t->_finalize_token->__TOKENIZER__on_char( $t );
}

# Our version of canonical is plain simple
sub canonical { $_[0]->content }

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
