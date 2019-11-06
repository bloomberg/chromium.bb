package PPI::Token::Quote::Single;

=pod

=head1 NAME

PPI::Token::Quote::Single - A 'single quote' token

=head1 INHERITANCE

  PPI::Token::Quote::Single
  isa PPI::Token::Quote
      isa PPI::Token
          isa PPI::Element

=head1 SYNOPSIS

  'This is a single quote'
  
  q{This is a literal, but NOT a single quote}

=head1 DESCRIPTION

A C<PPI::Token::Quote::Single> object represents a single quoted string
literal. 

=head1 METHODS

There are no methods available for C<PPI::Token::Quote::Single> beyond
those provided by the parent L<PPI::Token::Quote>, L<PPI::Token> and
L<PPI::Element> classes.

=cut

use strict;
use PPI::Token::Quote ();
use PPI::Token::_QuoteEngine::Simple ();

our $VERSION = '1.269'; # VERSION

our @ISA = qw{
	PPI::Token::_QuoteEngine::Simple
	PPI::Token::Quote
};





#####################################################################
# PPI::Token::Quote Methods

sub string {
	my $str = $_[0]->{content};
	substr( $str, 1, length($str) - 2 );
}


my %UNESCAPE = (
	"\\'"  => "'",
	"\\\\" => "\\",
);

sub literal {
	# Unescape \\ and \' ONLY
	my $str = $_[0]->string;
	$str =~ s/(\\.)/$UNESCAPE{$1} || $1/ge;
	return $str;
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
