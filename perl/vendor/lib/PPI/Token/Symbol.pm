package PPI::Token::Symbol;

=pod

=head1 NAME

PPI::Token::Symbol - A token class for variables and other symbols

=head1 INHERITANCE

  PPI::Token::Symbol
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

The C<PPI::Token::Symbol> class is used to cover all tokens that represent
variables and other things that start with a sigil.

=head1 METHODS

This class has several methods beyond what is provided by its
L<PPI::Token> and L<PPI::Element> parent classes.

Most methods are provided to help work out what the object is actually
pointing at, rather than what it might appear to be pointing at.

=cut
 
use strict;
use Params::Util qw{_INSTANCE};
use PPI::Token   ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}





#####################################################################
# PPI::Token::Symbol Methods

=pod

=head2 canonical

The C<canonical> method returns a normalized, canonical version of the
symbol.

For example, it converts C<$ ::foo'bar::baz> to C<$main::foo::bar::baz>.

This does not fully resolve the symbol, but merely removes syntax
variations.

=cut

sub canonical {
	my $symbol = shift->content;
	$symbol =~ s/\s+//;
	$symbol =~ s/(?<=[\$\@\%\&\*])::/main::/;
	$symbol =~ s/\'/::/g;
	$symbol;
}

=pod

=head2 symbol

The C<symbol> method returns the ACTUAL symbol this token refers to.

A token of C<$foo> might actually be referring to C<@foo>, if it is found
in the form C<$foo[1]>.

This method attempts to resolve these issues to determine the actual
symbol.

Returns the symbol as a string.

=cut

my %cast_which_trumps_braces = map { $_ => 1 } qw{ $ @ };

sub symbol {
	my $self   = shift;
	my $symbol = $self->canonical;

	# Immediately return the cases where it can't be anything else
	my $type = substr( $symbol, 0, 1 );
	return $symbol if $type eq '%';
	return $symbol if $type eq '&';

	# Unless the next significant Element is a structure, it's correct.
	my $after  = $self->snext_sibling;
	return $symbol unless _INSTANCE($after, 'PPI::Structure');

	# Process the rest for cases where it might actually be something else
	my $braces = $after->braces;
	return $symbol unless defined $braces;
	if ( $type eq '$' ) {

		# If it is cast to '$' or '@', that trumps any braces
		my $before = $self->sprevious_sibling;
		return $symbol if $before &&
			$before->isa( 'PPI::Token::Cast' ) &&
			$cast_which_trumps_braces{ $before->content };

		# Otherwise the braces rule
		substr( $symbol, 0, 1, '@' ) if $braces eq '[]';
		substr( $symbol, 0, 1, '%' ) if $braces eq '{}';

	} elsif ( $type eq '@' ) {
		substr( $symbol, 0, 1, '%' ) if $braces eq '{}';

	}

	$symbol;
}

=pod

=head2 raw_type

The C<raw_type> method returns the B<apparent> type of the symbol in the
form of its sigil.

Returns the sigil as a string.

=cut

sub raw_type {
	substr( $_[0]->content, 0, 1 );
}

=pod

=head2 symbol_type

The C<symbol_type> method returns the B<actual> type of the symbol in the
form of its sigil.

Returns the sigil as a string.

=cut

sub symbol_type {
	substr( $_[0]->symbol, 0, 1 );
}





#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_char {
	my $t = $_[1];

	# Suck in till the end of the symbol
	my $line = substr( $t->{line}, $t->{line_cursor} );
	if ( $line =~ /^([\w:\']+)/ ) {
		$t->{token}->{content} .= $1;
		$t->{line_cursor}      += length $1;
	}

	# Handle magic things
	my $content = $t->{token}->{content};	
	if ( $content eq '@_' or $content eq '$_' ) {
		$t->{class} = $t->{token}->set_class( 'Magic' );
		return $t->_finalize_token->__TOKENIZER__on_char( $t );
	}

	# Shortcut for most of the X:: symbols
	if ( $content eq '$::' ) {
		# May well be an alternate form of a Magic
		my $nextchar = substr( $t->{line}, $t->{line_cursor}, 1 );
		if ( $nextchar eq '|' ) {
			$t->{token}->{content} .= $nextchar;
			$t->{line_cursor}++;
			$t->{class} = $t->{token}->set_class( 'Magic' );
		}
		return $t->_finalize_token->__TOKENIZER__on_char( $t );
	}
	if ( $content =~ /^[\$%*@&]::(?:[^\w]|$)/ ) {
		my $current = substr( $content, 0, 3, '' );
		$t->{token}->{content} = $current;
		$t->{line_cursor} -= length( $content );
		return $t->_finalize_token->__TOKENIZER__on_char( $t );
	}
	if ( $content =~ /^(?:\$|\@)\d+/ ) {
		$t->{class} = $t->{token}->set_class( 'Magic' );
		return $t->_finalize_token->__TOKENIZER__on_char( $t );
	}

	# Trim off anything we oversucked...
	$content =~ /^(
		[\$@%&*]
		(?: : (?!:) | # Allow single-colon non-magic vars
			(?: \w+ | \' (?!\d) \w+ | \:: \w+ )
			(?:
				# Allow both :: and ' in namespace separators
				(?: \' (?!\d) \w+ | \:: \w+ )
			)*
			(?: :: )? # Technically a compiler-magic hash, but keep it here
		)
	)/x or return undef;
	unless ( length $1 eq length $content ) {
		$t->{line_cursor} += length($1) - length($content);
		$t->{token}->{content} = $1;
	}

	$t->_finalize_token->__TOKENIZER__on_char( $t );
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
