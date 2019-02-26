package PPI::Token::BOM;

=pod

=head1 NAME

PPI::Token::BOM - Tokens representing Unicode byte order marks

=head1 INHERITANCE

  PPI::Token::BOM
  isa PPI::Token
      isa PPI::Element

=head1 DESCRIPTION

This is a special token in that it can only occur at the beginning of
documents.  If a BOM byte mark occurs elsewhere in a file, it should
be treated as L<PPI::Token::Whitespace>.  We recognize the byte order
marks identified at this URL:
L<http://www.unicode.org/faq/utf_bom.html#BOM>

    UTF-32, big-endian     00 00 FE FF
    UTF-32, little-endian  FF FE 00 00
    UTF-16, big-endian     FE FF
    UTF-16, little-endian  FF FE
    UTF-8                  EF BB BF

Note that as of this writing, PPI only has support for UTF-8
(namely, in POD and strings) and no support for UTF-16 or UTF-32.  We
support the BOMs of the latter two for completeness only.

The BOM is considered non-significant, like white space.

=head1 METHODS

There are no additional methods beyond those provided by the parent
L<PPI::Token> and L<PPI::Element> classes.

=cut

use strict;
use PPI::Token ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '1.215';
	@ISA     = 'PPI::Token';
}

sub significant { '' }





#####################################################################
# Parsing Methods

my %bom_types = (
   "\x00\x00\xfe\xff" => 'UTF-32',
   "\xff\xfe\x00\x00" => 'UTF-32',
   "\xfe\xff"         => 'UTF-16',
   "\xff\xfe"         => 'UTF-16',
   "\xef\xbb\xbf"     => 'UTF-8',
);

sub __TOKENIZER__on_line_start {
	my $t = $_[1];
	$_ = $t->{line};

	if (m/^(\x00\x00\xfe\xff |  # UTF-32, big-endian
		\xff\xfe\x00\x00 |  # UTF-32, little-endian
		\xfe\xff         |  # UTF-16, big-endian
		\xff\xfe         |  # UTF-16, little-endian
		\xef\xbb\xbf)       # UTF-8
	    /xs) {
	   my $bom = $1;

	   if ($bom_types{$bom} ne 'UTF-8') {
	      return $t->_error("$bom_types{$bom} is not supported");
	   }

	   $t->_new_token('BOM', $bom) or return undef;
	   $t->{line_cursor} += length $bom;
	}

	# Continue just as if there was no BOM
	$t->{class} = 'PPI::Token::Whitespace';
	return $t->{class}->__TOKENIZER__on_line_start($t);
}

1;

=pod

=head1 SUPPORT

See the L<support section|PPI/SUPPORT> in the main module

=head1 AUTHOR

Chris Dolan E<lt>cdolan@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright 2001 - 2011 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
