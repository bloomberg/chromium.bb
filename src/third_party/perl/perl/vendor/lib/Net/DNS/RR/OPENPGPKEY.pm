package Net::DNS::RR::OPENPGPKEY;

#
# $Id: OPENPGPKEY.pm 1597 2017-09-22 08:04:02Z willem $
#
our $VERSION = (qw$LastChangedRevision: 1597 $)[1];


use strict;
use warnings;
use base qw(Net::DNS::RR);

=head1 NAME

Net::DNS::RR::OPENPGPKEY - DNS OPENPGPKEY resource record

=cut


use integer;

use MIME::Base64;


sub _decode_rdata {			## decode rdata from wire-format octet string
	my $self = shift;
	my ( $data, $offset ) = @_;

	my $length = $self->{rdlength};
	$self->keybin( substr $$data, $offset, $length );
}


sub _encode_rdata {			## encode rdata as wire-format octet string
	my $self = shift;

	pack 'a*', $self->keybin;
}


sub _format_rdata {			## format rdata portion of RR string.
	my $self = shift;

	my @base64 = split /\s+/, encode_base64( $self->keybin );
}


sub _parse_rdata {			## populate RR from rdata in argument list
	my $self = shift;

	$self->key(@_);
}


sub key {
	my $self = shift;
	return MIME::Base64::encode( $self->keybin(), "" ) unless scalar @_;
	$self->keybin( MIME::Base64::decode( join "", @_ ) );
}


sub keybin {
	my $self = shift;

	$self->{keybin} = shift if scalar @_;
	$self->{keybin} || "";
}


1;
__END__


=head1 SYNOPSIS

    use Net::DNS;
    $rr = new Net::DNS::RR('name OPENPGPKEY key');

=head1 DESCRIPTION

Class for OpenPGP Key (OPENPGPKEY) resource records.

=head1 METHODS

The available methods are those inherited from the base class augmented
by the type-specific methods defined in this package.

Use of undocumented package features or direct access to internal data
structures is discouraged and could result in program termination or
other unpredictable behaviour.


=head2 key

    $key = $rr->key;
    $rr->key( $key );

Base64 encoded representation of the OpenPGP public key material.

=head2 keybin

    $keybin = $rr->keybin;
    $rr->keybin( $keybin );

OpenPGP public key material consisting of
a single OpenPGP transferable public key in RFC4880 format.


=head1 COPYRIGHT

Copyright (c)2014 Dick Franks

All rights reserved.

Package template (c)2009,2012 O.M.Kolkman and R.W.Franks.


=head1 LICENSE

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted, provided
that the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation, and that the name of the author not be used in advertising
or publicity pertaining to distribution of the software without specific
prior written permission.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.


=head1 SEE ALSO

L<perl>, L<Net::DNS>, L<Net::DNS::RR>, RFC7929

=cut
