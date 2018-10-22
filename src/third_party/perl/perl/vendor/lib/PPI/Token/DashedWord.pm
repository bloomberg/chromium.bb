package PPI::Token::DashedWord;

=pod

=head1 NAME

PPI::Token::DashedWord - A dashed bareword token

=head1 INHERITANCE

  PPI::Token::DashedWord
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

The "dashed bareword" token represents literal values like C<-foo>.

NOTE: this class is currently unused.  All tokens that should be
PPI::Token::DashedWords are just normal PPI::Token::Word instead.
That actually makes sense, since there really is nothing special about
this class except that dashed words cannot be subroutine names or
keywords.  As such, this class may be removed from PPI in the future.

=head1 METHODS

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}

=pod

=head2 literal

Returns the value of the dashed word as a string.  This differs from
C<content> because C<-Foo'Bar> expands to C<-Foo::Bar>.

=begin testing literal 9

my @pairs = (
	"-foo",        '-foo',
	"-Foo::Bar",   '-Foo::Bar',
	"-Foo'Bar",    '-Foo::Bar',
);
while ( @pairs ) {
	my $from  = shift @pairs;
	my $to    = shift @pairs;
	my $doc   = PPI::Document->new( \"( $from => 1 );" );
	isa_ok( $doc, 'PPI::Document' );
	my $word = $doc->find_first('Token::DashedWord');
	SKIP: {
		skip( "PPI::Token::DashedWord is deactivated", 2 );
		isa_ok( $word, 'PPI::Token::DashedWord' );
		is( $word && $word->literal, $to, "The source $from becomes $to ok" );
	}
}

=end testing

=cut

*literal = *PPI::Token::Word::literal;



#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_char {
	my $t = $_[1];

	# Suck to the end of the dashed bareword
	my $line = substr( $t->{line}, $t->{line_cursor} );
	if ( $line =~ /^(\w+)/ ) {
		$t->{token}->{content} .= $1;
		$t->{line_cursor} += length $1;
	}

	# Are we a file test operator?
	if ( $t->{token}->{content} =~ /^\-[rwxoRWXOezsfdlpSbctugkTBMAC]$/ ) {
		# File test operator
		$t->{class} = $t->{token}->set_class( 'Operator' );
	} else {
		# No, normal dashed bareword
		$t->{class} = $t->{token}->set_class( 'Word' );
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
