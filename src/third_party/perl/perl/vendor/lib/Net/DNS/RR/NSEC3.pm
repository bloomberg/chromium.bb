package Net::DNS::RR::NSEC3;

#
# $Id: NSEC3.pm 1726 2018-12-15 12:59:56Z willem $
#
our $VERSION = (qw$LastChangedRevision: 1726 $)[1];


use strict;
use warnings;
use base qw(Net::DNS::RR::NSEC);

=head1 NAME

Net::DNS::RR::NSEC3 - DNS NSEC3 resource record

=cut


use integer;

use base qw(Exporter);
our @EXPORT_OK = qw(name2hash);

use Carp;

require Net::DNS::DomainName;

eval 'require Digest::SHA';		## optional for simple Net::DNS RR

my %digest = (
	'1' => ['Digest::SHA', 1],				# RFC3658
	);

{
	my @digestbyname = (
		'SHA-1' => 1,					# RFC3658
		);

	my @digestalias = ( 'SHA' => 1 );

	my %digestbyval = reverse @digestbyname;

	my @digestrehash = map /^\d/ ? ($_) x 3 : do { s/[\W_]//g; uc($_) }, @digestbyname;
	my %digestbyname = ( @digestalias, @digestrehash );	# work around broken cperl

	sub _digestbyname {
		my $name = shift;
		my $key	 = uc $name;				# synthetic key
		$key =~ s /[\W_]//g;				# strip non-alphanumerics
		$digestbyname{$key} || croak qq[unknown digest type "$name"];
	}

	sub _digestbyval {
		my $value = shift;
		$digestbyval{$value} || return $value;
	}
}


sub _decode_rdata {			## decode rdata from wire-format octet string
	my $self = shift;
	my ( $data, $offset ) = @_;

	my $limit = $offset + $self->{rdlength};
	my $ssize = unpack "\@$offset x4 C", $$data;
	my ( $algorithm, $flags, $iterations, $saltbin ) = unpack "\@$offset CCnx a$ssize", $$data;
	@{$self}{qw(algorithm flags iterations saltbin)} = ( $algorithm, $flags, $iterations, $saltbin );
	$offset += 5 + $ssize;
	my $hsize = unpack "\@$offset C", $$data;
	$self->{hnxtname} = unpack "\@$offset x a$hsize", $$data;
	$offset += 1 + $hsize;
	$self->{typebm} = substr $$data, $offset, ( $limit - $offset );
	$self->{hashfn} = _hashfn( $algorithm, $iterations, $saltbin );
}


sub _encode_rdata {			## encode rdata as wire-format octet string
	my $self = shift;

	my $salt = $self->saltbin;
	my $hash = $self->{hnxtname};
	pack 'CCn C a* C a* a*', $self->algorithm, $self->flags, $self->iterations,
			length($salt), $salt,
			length($hash), $hash,
			$self->{typebm};
}


sub _format_rdata {			## format rdata portion of RR string.
	my $self = shift;

	my @rdata = (
		$self->algorithm, $self->flags, $self->iterations,
		$self->salt || '-', $self->hnxtname, $self->typelist
		);
}


sub _parse_rdata {			## populate RR from rdata in argument list
	my $self = shift;

	my $alg = $self->algorithm(shift);
	$self->flags(shift);
	my $iter = $self->iterations(shift);
	my $salt = shift;
	$self->salt($salt) unless $salt eq '-';
	$self->hnxtname(shift);
	$self->typelist(@_);
	$self->{hashfn} = _hashfn( $alg, $iter, $self->{saltbin} );
}


sub _defaults {				## specify RR attribute default values
	my $self = shift;

	$self->_parse_rdata( 1, 0, 0, '' );
}


sub algorithm {
	my ( $self, $arg ) = @_;

	unless ( ref($self) ) {		## class method or simple function
		my $argn = pop;
		return $argn =~ /[^0-9]/ ? _digestbyname($argn) : _digestbyval($argn);
	}

	return $self->{algorithm} unless defined $arg;
	return _digestbyval( $self->{algorithm} ) if $arg =~ /MNEMONIC/i;
	return $self->{algorithm} = _digestbyname($arg);
}


sub flags {
	my $self = shift;

	$self->{flags} = 0 + shift if scalar @_;
	$self->{flags} || 0;
}


sub optout {
	my $bit = 0x01;
	for ( shift->{flags} ) {
		my $set = $bit | ( $_ ||= 0 );
		$_ = (shift) ? $set : ( $set ^ $bit ) if scalar @_;
		return $_ & $bit;
	}
}


sub iterations {
	my $self = shift;

	$self->{iterations} = 0 + shift if scalar @_;
	$self->{iterations} || 0;
}


sub salt {
	my $self = shift;
	return unpack "H*", $self->saltbin() unless scalar @_;
	$self->saltbin( pack "H*", map /[^\dA-F]/i ? croak "corrupt hex" : $_, join "", @_ );
}


sub saltbin {
	my $self = shift;

	$self->{saltbin} = shift if scalar @_;
	$self->{saltbin} || "";
}


sub hnxtname {
	my $self = shift;
	$self->{hnxtname} = _decode_base32hex(shift) if scalar @_;
	_encode_base32hex( $self->{hnxtname} ) if defined wantarray;
}


sub covers {
	my ( $self, $name ) = @_;

	my ( $owner, @zone ) = $self->{owner}->_wire;
	my $ownerhash = _decode_base32hex($owner);
	my $nexthash  = $self->{hnxtname};

	my @label = new Net::DNS::DomainName($name)->_wire;
	my @close = @label;
	foreach (@zone) { pop(@close) }				# strip zone labels
	return if lc($name) ne lc( join '.', @close, @zone );	# out of zone

	my $hashfn = $self->{hashfn};

	foreach (@close) {
		my $hash = &$hashfn( join '.', @label );
		my $cmp1 = $hash cmp $ownerhash;
		last unless $cmp1;				# stop at provable encloser
		return 1 if ( $cmp1 + ( $nexthash cmp $hash ) ) == 2;
		shift @label;
	}
	return;
}


sub covered {				## historical
	&covers;						# uncoverable pod
}

sub match {				## historical
	my ( $self, $name ) = @_;				# uncoverable pod

	my ($owner) = $self->{owner}->_wire;
	my $ownerhash = _decode_base32hex($owner);

	my $hashfn = $self->{hashfn};
	$ownerhash eq &$hashfn($name);
}


sub encloser {
	my ( $self, $qname ) = @_;

	my ( $owner, @zone ) = $self->{owner}->_wire;
	my $ownerhash = _decode_base32hex($owner);
	my $nexthash  = $self->{hnxtname};

	my @label = new Net::DNS::DomainName($qname)->_wire;
	my @close = @label;
	foreach (@zone) { pop(@close) }				# strip zone labels
	return if lc($qname) ne lc( join '.', @close, @zone );	# out of zone

	my $hashfn = $self->{hashfn};

	my $encloser = $qname;
	shift @label;
	foreach (@close) {
		my $nextcloser = $encloser;
		my $hash = &$hashfn( $encloser = join '.', @label );
		shift @label;
		next if $hash ne $ownerhash;
		$self->{nextcloser} = $nextcloser;		# next closer name
		$self->{wildcard} = join '.', '*', $encloser;	# wildcard at provable encloser
		return $encloser;				# provable encloser
	}
	return;
}


sub nextcloser { return shift->{nextcloser}; }

sub wildcard { return shift->{wildcard}; }


########################################

sub _decode_base32hex {
	local $_ = shift || '';
	tr [0-9A-Va-v\060-\071\101-\126\141-\166] [\000-\037\012-\037\000-\037\012-\037];
	my $l = ( 5 * length ) & ~7;
	pack "B$l", join '', map unpack( 'x3a5', unpack 'B8', $_ ), split //;
}


sub _encode_base32hex {
	my @split = grep length, split /(\S{5})/, unpack 'B*', shift;
	local $_ = join '', map pack( 'B*', "000$_" ), @split;
	tr [\000-\037] [0-9a-v];
	return $_;
}


my ( $cache1, $cache2, $limit ) = ( {}, {}, 10 );

sub _hashfn {
	my $hashalg    = shift;
	my $iterations = shift || 0;
	my $salt       = shift || '';

	my $key_adjunct = pack 'Cna*', $hashalg, $iterations, $salt;
	$iterations++;

	my $instance = eval {
		my $arglist = $digest{$hashalg};
		my ( $class, @argument ) = @$arglist;
		$class->new(@argument);
	};
	my $exception = $@;

	return $exception ? sub { croak $exception } : sub {
		my $name  = new Net::DNS::DomainName(shift)->canonical;
		my $key	  = join '', $name, $key_adjunct;
		my $cache = $$cache1{$key} ||= $$cache2{$key};	# two layer cache
		return $cache if defined $cache;
		( $cache1, $cache2, $limit ) = ( {}, $cache1, 50 ) unless $limit--;    # recycle cache

		my $hash = $name;
		my $iter = $iterations;
		$instance->reset;
		while ( $iter-- ) {
			$instance->add($hash);
			$instance->add($salt);
			$hash = $instance->digest;
		}
		return $$cache1{$key} = $hash;
	};
}


sub hashalgo { &algorithm; }					# uncoverable pod

sub name2hash {
	my $hashalg    = shift;					# uncoverable pod
	my $name       = shift;
	my $iterations = shift || 0;
	my $salt       = pack 'H*', shift || '';
	my $hash       = _hashfn( $hashalg, $iterations, $salt );
	_encode_base32hex( &$hash($name) );
}


1;
__END__


=head1 SYNOPSIS

    use Net::DNS;
    $rr = new Net::DNS::RR('name NSEC3 algorithm flags iterations salt hnxtname');

=head1 DESCRIPTION

Class for DNSSEC NSEC3 resource records.

The NSEC3 Resource Record (RR) provides authenticated denial of
existence for DNS Resource Record Sets.

The NSEC3 RR lists RR types present at the original owner name of the
NSEC3 RR.  It includes the next hashed owner name in the hash order
of the zone.  The complete set of NSEC3 RRs in a zone indicates which
RRSets exist for the original owner name of the RR and form a chain
of hashed owner names in the zone.

=head1 METHODS

The available methods are those inherited from the base class augmented
by the type-specific methods defined in this package.

Use of undocumented package features or direct access to internal data
structures is discouraged and could result in program termination or
other unpredictable behaviour.


=head2 algorithm

    $algorithm = $rr->algorithm;
    $rr->algorithm( $algorithm );

The Hash Algorithm field is represented as an unsigned decimal
integer.  The value has a maximum of 255.

algorithm() may also be invoked as a class method or simple function
to perform mnemonic and numeric code translation.

=head2 flags

    $flags = $rr->flags;
    $rr->flags( $flags );

The Flags field is an unsigned decimal integer
interpreted as eight concatenated Boolean values. 

=over 4

=item optout

 $rr->optout(1);

 if ( $rr->optout ) {
	...
 }

Boolean Opt Out flag.

=back

=head2 iterations

    $iterations = $rr->iterations;
    $rr->iterations( $iterations );

The Iterations field is represented as an unsigned decimal
integer.  The value is between 0 and 65535, inclusive. 

=head2 salt

    $salt = $rr->salt;
    $rr->salt( $salt );

The Salt field is represented as a contiguous sequence of hexadecimal
digits. A "-" (unquoted) is used in string format to indicate that the
salt field is absent. 

=head2 saltbin

    $saltbin = $rr->saltbin;
    $rr->saltbin( $saltbin );

The Salt field as a sequence of octets. 

=head2 hnxtname

    $hnxtname = $rr->hnxtname;
    $rr->hnxtname( $hnxtname );

The Next Hashed Owner Name field points to the next node that has
authoritative data or contains a delegation point NS RRset.

=head2 typelist

    @typelist = $rr->typelist;
    $typelist = $rr->typelist;
    $rr->typelist( @typelist );

typelist() identifies the RRset types that exist at the domain name
matched by the NSEC3 RR.  When called in scalar context, the list is
interpolated into a string.

=head2 typemap

    $exists = $rr->typemap($rrtype);

typemap() returns a Boolean true value if the specified RRtype occurs
in the type bitmap of the NSEC3 record.

=head2 covers

    $covered = $rr->covers( 'example.foo' );

covers() returns a Boolean true value if the hash of the domain name
argument, or ancestor of that name, falls between the owner name and
the next hashed owner name of the NSEC3 RR.

=head2 encloser, nextcloser, wildcard

    $encloser = $rr->encloser( 'example.foo' );
    print "encloser: $encloser\n" if $encloser;

encloser() returns the name of a provable encloser of the query name
argument obtained from the NSEC3 RR.

nextcloser() returns the next closer name, which is one label longer
than the closest encloser.
This is only valid after encloser() has returned a valid domain name.

wildcard() returns the unexpanded wildcard name from which the next
closer name was possibly synthesised.
This is only valid after encloser() has returned a valid domain name.


=head1 COPYRIGHT

Copyright (c)2017,2018 Dick Franks

Portions Copyright (c)2007,2008 NLnet Labs.  Author Olaf M. Kolkman

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

L<perl>, L<Net::DNS>, L<Net::DNS::RR>, RFC5155, RFC4648

L<Hash Algorithms|http://www.iana.org/assignments/dnssec-nsec3-parameters>

=cut
