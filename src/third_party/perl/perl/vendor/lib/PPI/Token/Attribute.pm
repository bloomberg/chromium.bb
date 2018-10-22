package PPI::Token::Attribute;

=pod

=head1 NAME

PPI::Token::Attribute - A token for a subroutine attribute

=head1 INHERITANCE

  PPI::Token::Attribute
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

In Perl, attributes are a relatively recent addition to the language.

Given the code C< sub foo : bar(something) {} >, the C<bar(something)>
part is the attribute.

A C<PPI::Token::Attribute> token represents the entire of the attribute,
as the braces and its contents are not parsed into the tree, and are
treated by Perl (and thus by us) as a single string.

=head1 METHODS

This class provides some additional methods beyond those provided by its
L<PPI::Token> and L<PPI::Element> parent classes.

Got any ideas for methods? Submit a report to rt.cpan.org!

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}




#####################################################################
# PPI::Token::Attribute Methods

=pod

=head2 identifier

The C<identifier> attribute returns the identifier part of the attribute.

That is, for the attribute C<foo(bar)>, the C<identifier> method would
return C<"foo">.

=cut

sub identifier {
	my $self = shift;
	$self->{content} =~ /^(.+?)\(/ ? $1 : $self->{content};
}

=pod

=head2 parameters

The C<parameters> method returns the parameter strong for the attribute.

That is, for the attribute C<foo(bar)>, the C<parameters> method would
return C<"bar">.

Returns the parameters as a string (including the null string C<''> for
the case of an attribute such as C<foo()>.

Returns C<undef> if the attribute does not have parameters.

=cut

sub parameters {
	my $self = shift;
	$self->{content} =~ /\((.+)\)$/ ? $1 : undef;
}





#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_char {
	my $class = shift;
	my $t     = shift;
	my $char  = substr( $t->{line}, $t->{line_cursor}, 1 );

	# Unless this is a '(', we are finished.
	unless ( $char eq '(' ) {
		# Finalise and recheck
		return $t->_finalize_token->__TOKENIZER__on_char( $t );
	}

	# This is a bar(...) style attribute.
	# We are currently on the ( so scan in until the end.
	# We finish on the character AFTER our end
	my $string = $class->__TOKENIZER__scan_for_end( $t );
	if ( ref $string ) {
		# EOF
		$t->{token}->{content} .= $$string;
		$t->_finalize_token;
		return 0;
	}

	# Found the end of the attribute
	$t->{token}->{content} .= $string;
	$t->_finalize_token->__TOKENIZER__on_char( $t );
}

# Scan for a close braced, and take into account both escaping,
# and open close bracket pairs in the string. When complete, the
# method leaves the line cursor on the LAST character found.
sub __TOKENIZER__scan_for_end {
	my $t = $_[1];

	# Loop as long as we can get new lines
	my $string = '';
	my $depth = 0;
	while ( exists $t->{line} ) {
		# Get the search area
		my $search = $t->{line_cursor}
			? substr( $t->{line}, $t->{line_cursor} )
			: $t->{line};

		# Look for a match
		unless ( $search =~ /^((?:\\.|[^()])*?[()])/ ) {
			# Load in the next line and push to first character
			$string .= $search;
			$t->_fill_line(1) or return \$string;
			$t->{line_cursor} = 0;
			next;
		}

		# Add to the string
		$string .= $1;
		$t->{line_cursor} += length $1;

		# Alter the depth and continue if we arn't at the end
		$depth += ($1 =~ /\($/) ? 1 : -1 and next;

		# Found the end
		return $string;
	}

	# Returning the string as a reference indicates EOF
	\$string;
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
