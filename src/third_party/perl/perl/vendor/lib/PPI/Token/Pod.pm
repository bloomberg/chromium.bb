package PPI::Token::Pod;

=pod

=head1 NAME

PPI::Token::Pod - Sections of POD in Perl documents

=head1 INHERITANCE

  PPI::Token::Pod
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

A single C<PPI::Token::Pod> object represents a complete section of POD
documentation within a Perl document.

=head1 METHODS

This class provides some additional methods beyond those provided by its
L<PPI::Token> and L<PPI::Element> parent classes.

Got any ideas for more methods? Submit a report to rt.cpan.org!

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
# PPI::Token::Pod Methods

=pod

=head2 merge @podtokens

The C<merge> constructor takes a number of C<PPI::Token::Pod> objects,
and returns a new object that represents one combined POD block with
the content of all of them.

Returns a new C<PPI::Token::Pod> object, or C<undef> on error.

=begin testing merge after PPI::Node 4

# Create the test fragments
my $one = PPI::Token::Pod->new("=pod\n\nOne\n\n=cut\n");
my $two = PPI::Token::Pod->new("=pod\n\nTwo");
isa_ok( $one, 'PPI::Token::Pod' );
isa_ok( $two, 'PPI::Token::Pod' );

# Create the combined Pod
my $merged = PPI::Token::Pod->merge($one, $two);
isa_ok( $merged, 'PPI::Token::Pod' );
is( $merged->content, "=pod\n\nOne\n\nTwo\n\n=cut\n", 'Merged POD looks ok' );

=end testing

=cut

sub merge {
	my $class = (! ref $_[0]) ? shift : return undef;

	# Check there are no bad arguments
	if ( grep { ! _INSTANCE($_, 'PPI::Token::Pod') } @_ ) {
		return undef;
	}

	# Get the tokens, and extract the lines
	my @content = ( map { [ $_->lines ] } @_ ) or return undef;

	# Remove the leading =pod tags, trailing =cut tags, and any empty lines
	# between them and the pod contents.
	foreach my $pod ( @content ) {
		# Leading =pod tag
		if ( @$pod and $pod->[0] =~ /^=pod\b/o ) {
			shift @$pod;
		}

		# Trailing =cut tag
		if ( @$pod and $pod->[-1] =~ /^=cut\b/o ) {
			pop @$pod;
		}

		# Leading and trailing empty lines
		while ( @$pod and $pod->[0]  eq '' ) { shift @$pod }
		while ( @$pod and $pod->[-1] eq '' ) { pop @$pod   }
	}

	# Remove any empty pod sections, and add the =pod and =cut tags
	# for the merged pod back to it.
	@content = ( [ '=pod' ], grep { @$_ } @content, [ '=cut' ] );

	# Create the new object
	$class->new( join "\n", map { join( "\n", @$_ ) . "\n" } @content );
}

=pod

=head2 lines

The C<lines> method takes the string of POD and breaks it into lines,
returning them as a list.

=cut

sub lines {
	split /(?:\015{1,2}\012|\015|\012)/, $_[0]->{content};
}






#####################################################################
# PPI::Element Methods

### XS -> PPI/XS.xs:_PPI_Token_Pod__significant 0.900+
sub significant { '' }





#####################################################################
# Tokenizer Methods

sub __TOKENIZER__on_line_start {
	my $t = $_[1];

	# Add the line to the token first
	$t->{token}->{content} .= $t->{line};

	# Check the line to see if it is a =cut line
	if ( $t->{line} =~ /^=(\w+)/ ) {
		# End of the token
		$t->_finalize_token if lc $1 eq 'cut';
	}

	0;
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
