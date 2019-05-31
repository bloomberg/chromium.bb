# $Id$
#
# This is free software, you may use it and distribute it under the same terms as
# Perl itself.
#
# Copyright 2001-2003 AxKit.com Ltd., 2002-2006 Christian Glahn, 2006-2009 Petr Pajas
#
#

package XML::LibXML::SAX;

use strict;
use warnings;

use vars qw($VERSION @ISA);

$VERSION = "2.0200"; # VERSION TEMPLATE: DO NOT CHANGE

use XML::LibXML;
use XML::SAX::Base;

use parent qw(XML::SAX::Base);

use Carp;
use IO::File;

sub CLONE_SKIP {
  return $XML::LibXML::__threads_shared ? 0 : 1;
}

sub set_feature {
	my ($self, $feat, $val) = @_;

	if ($feat eq 'http://xmlns.perl.org/sax/join-character-data') {
		$self->{JOIN_CHARACTERS} = $val;
		return 1;
	}

	shift(@_);
	return $self->SUPER::set_feature(@_);
}

sub _parse_characterstream {
    my ( $self, $fh ) = @_;
    # this my catch the xml decl, so the parser won't get confused about
    # a possibly wrong encoding.
    croak( "not implemented yet" );
}

sub _parse_bytestream {
    my ( $self, $fh ) = @_;
    $self->{ParserOptions}{LibParser}      = XML::LibXML->new()     unless defined $self->{ParserOptions}{LibParser};
    $self->{ParserOptions}{ParseFunc}      = \&XML::LibXML::parse_fh;
    $self->{ParserOptions}{ParseFuncParam} = $fh;
    $self->_parse;
    return $self->end_document({});
}

sub _parse_string {
    my ( $self, $string ) = @_;
    $self->{ParserOptions}{LibParser}      = XML::LibXML->new()     unless defined $self->{ParserOptions}{LibParser};
    $self->{ParserOptions}{ParseFunc}      = \&XML::LibXML::parse_string;
    $self->{ParserOptions}{ParseFuncParam} = $string;
    $self->_parse;
    return $self->end_document({});
}

sub _parse_systemid {
    my $self = shift;
    $self->{ParserOptions}{LibParser}      = XML::LibXML->new()     unless defined $self->{ParserOptions}{LibParser};
    $self->{ParserOptions}{ParseFunc}      = \&XML::LibXML::parse_file;
    $self->{ParserOptions}{ParseFuncParam} = shift;
    $self->_parse;
    return $self->end_document({});
}

sub parse_chunk {
    my ( $self, $chunk ) = @_;
    $self->{ParserOptions}{LibParser}      = XML::LibXML->new()     unless defined $self->{ParserOptions}{LibParser};
    $self->{ParserOptions}{ParseFunc}      = \&XML::LibXML::parse_xml_chunk;
    $self->{ParserOptions}{LibParser}->{IS_FILTER}=1; # a hack to prevent parse_xml_chunk from issuing end_document
    $self->{ParserOptions}{ParseFuncParam} = $chunk;
    $self->_parse;
    return;
}

sub _parse {
    my $self = shift;
    my $args = bless $self->{ParserOptions}, ref($self);

    if (defined($self->{JOIN_CHARACTERS})) {
    	$args->{LibParser}->{JOIN_CHARACTERS} = $self->{JOIN_CHARACTERS};
    } else {
    	$args->{LibParser}->{JOIN_CHARACTERS} = 0;
    }

    $args->{LibParser}->set_handler( $self );
    eval {
      $args->{ParseFunc}->($args->{LibParser}, $args->{ParseFuncParam});
    };

    if ( $args->{LibParser}->{SAX}->{State} == 1 ) {
        croak( "SAX Exception not implemented, yet; Data ended before document ended\n" );
    }

    # break a possible circular reference
    $args->{LibParser}->set_handler( undef );
    if ( $@ ) {
        croak $@;
    }
    return;
}

1;

