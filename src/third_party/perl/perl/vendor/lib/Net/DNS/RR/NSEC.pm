package Net::DNS::RR::NSEC;

#
# $Id: NSEC.pm 1696 2018-07-20 16:15:11Z willem $
#
our $VERSION = (qw$LastChangedRevision: 1696 $)[1];


use strict;
use warnings;
use base qw(Net::DNS::RR);

=head1 NAME

Net::DNS::RR::NSEC - DNS NSEC resource record

=cut


use integer;

use Net::DNS::DomainName;
use Net::DNS::Parameters;


sub _decode_rdata {			## decode rdata from wire-format octet string
	my $self = shift;
	my ( $data, $offset ) = @_;

	my $limit = $offset + $self->{rdlength};
	( $self->{nxtdname}, $offset ) = decode Net::DNS::DomainName(@_);
	$self->{typebm} = substr $$data, $offset, $limit - $offset;
}


sub _encode_rdata {			## encode rdata as wire-format octet string
	my $self = shift;

	my $nxtdname = $self->{nxtdname};
	join '', $nxtdname->encode(), $self->{typebm};
}


sub _format_rdata {			## format rdata portion of RR string.
	my $self = shift;

	my $nxtdname = $self->{nxtdname};
	my @rdata = ( $nxtdname->string(), $self->typelist );
}


sub _parse_rdata {			## populate RR from rdata in argument list
	my $self = shift;

	$self->nxtdname(shift);
	$self->typelist(@_);
}


sub _defaults {				## specify RR attribute default values
	my $self = shift;

	$self->_parse_rdata('.');
}


sub nxtdname {
	my $self = shift;

	$self->{nxtdname} = new Net::DNS::DomainName(shift) if scalar @_;
	$self->{nxtdname}->name if $self->{nxtdname};
}


sub typelist {
	my $self = shift;

	if ( scalar(@_) || !defined(wantarray) ) {
		$self->{typebm} = &_type2bm;
		return;
	}

	my @type = &_bm2type( $self->{typebm} );
	return wantarray ? (@type) : "@type";
}


sub typemap {
	my $self = shift;

	my $number = typebyname(shift);
	my $window = $number >> 8;
	my $bitnum = $number & 255;

	my $typebm = $self->{typebm} || return;
	my @bitmap;
	my $index = 0;
	while ( $index < length $typebm ) {
		my ( $block, $size ) = unpack "\@$index C2", $typebm;
		$bitmap[$block] = unpack "\@$index xxa$size", $typebm;
		$index += $size + 2;
	}

	my @bit = split //, unpack 'B*', ( $bitmap[$window] || return );
	return $bit[$bitnum];
}


sub covers {
	my $self = shift;
	my $name = join chr(0), reverse Net::DNS::DomainName->new(shift)->_wire;
	my $this = join chr(0), reverse $self->{owner}->_wire;
	my $next = join chr(0), reverse $self->{nxtdname}->_wire;
	foreach ( $name, $this, $next ) {tr /\101-\132/\141-\172/}

	return ( $name cmp $this ) + ( "$next\001" cmp $name ) == 2 unless $next gt $this;
	return ( $name cmp $this ) + ( $next cmp $name ) == 2;
}


########################################

sub _type2bm {
	my @typearray;
	foreach my $typename ( map split(), @_ ) {
		my $number = typebyname($typename);
		my $window = $number >> 8;
		my $bitnum = $number & 255;
		my $octet  = $bitnum >> 3;
		my $bit	   = $bitnum & 7;
		$typearray[$window][$octet] |= 0x80 >> $bit;
	}

	my $bitmap = '';
	my $window = 0;
	foreach (@typearray) {
		if ( my $pane = $typearray[$window] ) {
			my @content = map $_ || 0, @$pane;
			$bitmap .= pack 'CC C*', $window, scalar(@content), @content;
		}
		$window++;
	}

	return $bitmap;
}


sub _bm2type {
	my @typelist;
	my $bitmap = shift || return @typelist;

	my $index = 0;
	my $limit = length $bitmap;

	while ( $index < $limit ) {
		my ( $block, $size ) = unpack "\@$index C2", $bitmap;
		my $typenum = $block << 8;
		foreach my $octet ( unpack "\@$index xxC$size", $bitmap ) {
			my $i = $typenum += 8;
			my @name;
			while ($octet) {
				--$i;
				unshift @name, typebyval($i) if $octet & 1;
				$octet = $octet >> 1;
			}
			push @typelist, @name;
		}
		$index += $size + 2;
	}

	return @typelist;
}


sub typebm {				## historical
	my $self = shift;					# uncoverable pod
	$self->{typebm} = shift if scalar @_;
	return $self->{typebm};
}


1;
__END__


=head1 SYNOPSIS

    use Net::DNS;
    $rr = new Net::DNS::RR( 'name NSEC nxtdname typelist' );

=head1 DESCRIPTION

Class for DNSSEC NSEC resource records.

=head1 METHODS

The available methods are those inherited from the base class augmented
by the type-specific methods defined in this package.

Use of undocumented package features or direct access to internal data
structures is discouraged and could result in program termination or
other unpredictable behaviour.


=head2 nxtdname

    $nxtdname = $rr->nxtdname;
    $rr->nxtdname( $nxtdname );

The Next Domain field contains the next owner name (in the
canonical ordering of the zone) that has authoritative data
or contains a delegation point NS RRset.

=head2 typelist

    @typelist = $rr->typelist;
    $typelist = $rr->typelist;

typelist() identifies the RRset types that exist at the NSEC RR
owner name.  When called in scalar context, the list is interpolated
into a string.

=head2 typemap

    $exists = $rr->typemap($rrtype);

typemap() returns a Boolean true value if the specified RRtype occurs
in the type bitmap of the NSEC record.

=head2 covers

    $covered = $rr->covers( 'example.foo' );

covers() returns a Boolean true value if the canonical form of the name,
or one of its ancestors, falls between the owner name and the nxtdname
field of the NSEC record.


=head1 COPYRIGHT

Copyright (c)2001-2005 RIPE NCC.  Author Olaf M. Kolkman

Portions Copyright (c)2018 Dick Franks

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

L<perl>, L<Net::DNS>, L<Net::DNS::RR>, RFC4034, RFC3755

=cut
