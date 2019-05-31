package Net::DNS::RR;

#
# $Id: RR.pm 1726 2018-12-15 12:59:56Z willem $
#
our $VERSION = (qw$LastChangedRevision: 1726 $)[1];


=head1 NAME

Net::DNS::RR - DNS resource record base class

=head1 SYNOPSIS

    use Net::DNS;

    $rr = new Net::DNS::RR('example.com IN AAAA 2001:DB8::1');

    $rr = new Net::DNS::RR(
	    owner   => 'example.com',
	    type    => 'AAAA',
	    address => '2001:DB8::1'
	    );


=head1 DESCRIPTION

Net::DNS::RR is the base class for DNS Resource Record (RR) objects.
See also the manual pages for each specific RR type.

=cut


use strict;
use warnings;
use integer;
use Carp;

use constant LIB => grep $_ ne '.', grep !ref($_), @INC;

use Net::DNS::Parameters;
use Net::DNS::DomainName;


=head1 METHODS

B<WARNING!!!>  Do not assume the RR objects you receive from a query
are of a particular type.  You must always check the object type
before calling any of its methods.  If you call an unknown method,
you will get an error message and execution will be terminated.

=cut

sub new {
	return eval {
		local $SIG{__DIE__};
		scalar @_ > 2 ? &_new_hash : &_new_string;
	} || do {
		my $class = shift || __PACKAGE__;
		my @param = map defined($_) ? split /\s+/ : 'undef', @_;
		my $stmnt = substr "new $class( @param )", 0, 80;
		croak "${@}in $stmnt\n";
	};
}


=head2 new (from string)

    $aaaa  = new Net::DNS::RR('host.example.com. 86400 AAAA 2001:DB8::1');
    $mx	   = new Net::DNS::RR('example.com. 7200 MX 10 mailhost.example.com.');
    $cname = new Net::DNS::RR('www.example.com 300 IN CNAME host.example.com');
    $txt   = new Net::DNS::RR('txt.example.com 3600 HS TXT "text data"');

Returns an object of the appropriate RR type, or a L<Net::DNS::RR> object
if the type is not implemented. The attribute values are extracted from the
string passed by the user. The syntax of the argument string follows the
RFC1035 specification for zone files, and is compatible with the result
returned by the string method.

The owner and RR type are required; all other information is optional.
Omitting the optional fields is useful for creating the empty RDATA
sections required for certain dynamic update operations.
See the L<Net::DNS::Update> manual page for additional examples.

All names are interpreted as fully qualified domain names.
The trailing dot (.) is optional.

=cut

my $PARSE_REGEX = q/("[^"]*")|;[^\n]*|[ \t\n\r\f()]/;

sub _new_string {
	my $base;
	local $_;
	( $base, $_ ) = @_;
	croak 'argument absent or undefined' unless defined $_;
	croak 'non-scalar argument' if ref $_;

	# parse into quoted strings, contiguous non-whitespace and (discarded) comments
	s/\\\\/\\092/g;						# disguise escaped escape
	s/\\"/\\034/g;						# disguise escaped quote
	s/\\\(/\\040/g;						# disguise escaped bracket
	s/\\\)/\\041/g;						# disguise escaped bracket
	s/\\;/\\059/g;						# disguise escaped semicolon
	my ( $owner, @token ) = grep defined && length, split /$PARSE_REGEX/o;

	croak 'unable to parse RR string' unless scalar @token;
	my $t1 = uc $token[0];
	my $t2 = $token[1];

	my ( $ttl, $class );
	if ( not defined $t2 ) {				# <owner> <type>
		@token = ('ANY') if $classbyname{$t1};		# <owner> <class>
	} elsif ( $t1 =~ /^\d/ ) {
		$ttl = shift @token;				# <owner> <ttl> [<class>] <type>
		$class = shift @token if $classbyname{uc $t2} || $t2 =~ /^CLASS\d/i;
	} elsif ( $classbyname{$t1} || $t1 =~ /^CLASS\d/ ) {
		$class = shift @token;				# <owner> <class> [<ttl>] <type>
		$ttl = shift @token if $t2 =~ /^\d/;
	}

	my $type      = shift(@token);
	my $populated = scalar @token;

	my $self = $base->_subclass( $type, $populated );	# create RR object
	$self->owner($owner);
	$self->class($class) if defined $class;			# specify CLASS
	$self->ttl($ttl)     if defined $ttl;			# specify TTL

	return $self unless $populated;				# empty RR

	if ( $#token && $token[0] =~ /^[\\]?#$/ ) {
		shift @token;					# RFC3597 hexadecimal format
		my $rdlen = shift(@token) || 0;
		my $rdata = pack 'H*', join( '', @token );
		croak 'length and hexadecimal data inconsistent' unless $rdlen == length $rdata;
		$self->rdata($rdata);				# unpack RDATA
		return $self;
	}

	$self->_parse_rdata(@token);				# parse arguments
	return $self;
}


=head2 new (from hash)

    $rr = new Net::DNS::RR(%hash);

    $rr = new Net::DNS::RR(
	    owner   => 'host.example.com',
	    ttl	    => 86400,
	    class   => 'IN',
	    type    => 'AAAA',
	    address => '2001:DB8::1'
	    );
 
    $rr = new Net::DNS::RR(
	    owner   => 'txt.example.com',
	    type    => 'TXT',
	    txtdata => [ 'one', 'two' ]
	    );

Returns an object of the appropriate RR type, or a L<Net::DNS::RR> object
if the type is not implemented. Consult the relevant manual pages for the
usage of type specific attributes.

The owner and RR type are required; all other information is optional.
Omitting optional attributes is useful for creating the empty RDATA
sections required for certain dynamic update operations.

=cut

my @core = qw(owner name type class ttl rdlength);

sub _new_hash {
	my $base = shift;

	my %attribute = ( owner => '.', type => 'NULL' );
	while ( my $key = shift ) {
		$attribute{lc $key} = shift;
	}

	my ( $owner, $name, $type, $class, $ttl ) = delete @attribute{@core};

	my $self = $base->_subclass( $type, scalar(%attribute) );
	$self->owner( $name ? $name : $owner );
	$self->class($class) if defined $class;			# optional CLASS
	$self->ttl($ttl)     if defined $ttl;			# optional TTL

	eval {
		while ( my ( $attribute, $value ) = each %attribute ) {
			$self->$attribute( ref($value) eq 'ARRAY' ? @$value : $value );
		}
	};
	die ref($self) eq __PACKAGE__ ? "type $type not implemented" : () if $@;

	return $self;
}


=head2 decode

    ( $rr, $next ) = decode Net::DNS::RR( \$data, $offset, @opaque );

Decodes a DNS resource record at the specified location within a
DNS packet.

The argument list consists of a reference to the buffer containing
the packet data and offset indicating where resource record begins.
Remaining arguments, if any, are passed as opaque data to
subordinate decoders.

Returns a C<Net::DNS::RR> object and the offset of the next record
in the packet.

An exception is raised if the data buffer contains insufficient or
corrupt data.

Any remaining arguments are passed as opaque data to subordinate
decoders and do not form part of the published interface.

=cut

use constant RRFIXEDSZ => length pack 'n2 N n', (0) x 4;

sub decode {
	my $base = shift;
	my ( $data, $offset, @opaque ) = @_;

	my ( $owner, $fixed ) = decode Net::DNS::DomainName1035(@_);

	my $index = $fixed + RRFIXEDSZ;
	die 'corrupt wire-format data' if length $$data < $index;
	my $self = $base->_subclass( unpack "\@$fixed n", $$data );
	$self->{owner} = $owner;
	@{$self}{qw(class ttl rdlength)} = unpack "\@$fixed x2 n N n", $$data;

	my $next = $index + $self->{rdlength};
	die 'corrupt wire-format data' if length $$data < $next;

	$self->{offset} = $offset || 0;
	$self->_decode_rdata( $data, $index, @opaque ) if $next > $index or $self->type eq 'OPT';
	delete $self->{offset};

	return wantarray ? ( $self, $next ) : $self;
}


=head2 encode

    $data = $rr->encode( $offset, @opaque );

Returns the C<Net::DNS::RR> in binary format suitable for inclusion
in a DNS packet buffer.

The offset indicates the intended location within the packet data
where the C<Net::DNS::RR> is to be stored.

Any remaining arguments are opaque data which are passed intact to
subordinate encoders.

=cut

sub encode {
	my $self = shift;
	my ( $offset, @opaque ) = scalar(@_) ? @_ : ( 0x4000, {} );

	my $owner = $self->{owner}->encode( $offset, @opaque );
	my ( $type, $class, $ttl ) = @{$self}{qw(type class ttl)};
	my $rdata = $self->_empty ? '' : $self->_encode_rdata( $offset + length($owner) + RRFIXEDSZ, @opaque );
	pack 'a* n2 N n a*', $owner, $type, $class || 1, $ttl || 0, length $rdata, $rdata;
}


=head2 canonical

    $data = $rr->canonical;

Returns the C<Net::DNS::RR> in canonical binary format suitable for
DNSSEC signature validation.

The absence of the associative array argument signals to subordinate
encoders that the canonical uncompressed lower case form of embedded
domain names is to be used.

=cut

sub canonical {
	my $self = shift;

	my $owner = $self->{owner}->canonical;
	my ( $type, $class, $ttl ) = @{$self}{qw(type class ttl)};
	my $rdata = $self->_empty ? '' : $self->_encode_rdata( length($owner) + RRFIXEDSZ );
	pack 'a* n2 N n a*', $owner, $type, $class || 1, $ttl || 0, length $rdata, $rdata;
}


=head2 print

    $rr->print;

Prints the resource record to the currently selected output filehandle.
Calls the string method to get the formatted RR representation.

=cut

sub print {
	print shift->string, "\n";
}


=head2 string

    print $rr->string, "\n";

Returns a string representation of the RR using the master file format
mandated by RFC1035.
All domain names are fully qualified with trailing dot.
This differs from RR attribute methods, which omit the trailing dot.

=cut

sub string {
	my $self = shift;

	my $name = $self->{owner}->string;
	my @ttl	 = grep defined, $self->{ttl};
	my @core = ( $name, @ttl, $self->class, $self->type );

	my $empty = $self->_empty;
	my @rdata = $empty ? () : eval { $self->_format_rdata };
	carp $@ if $@;

	my $tab = length($name) < 72 ? "\t" : ' ';
	$self->_annotation('no data') if $empty;

	my @line = _wrap( join( $tab, @core, '(' ), @rdata, ')' );

	my $last = pop(@line);					# last or only line
	$last = join $tab, @core, "@rdata" unless scalar(@line);

	return join "\n\t", @line, _wrap( $last, map "; $_", $self->_annotation );
}


=head2 plain

    $plain = $rr->plain;

Returns a simplified single-line representation of the RR.
This facilitates interaction with programs like nsupdate
which have rudimentary parsers.

=cut

sub plain {
	join ' ', shift->token;
}


=head2 token

    @token = $rr->token;

Returns a token list representation of the RR zone file string.

=cut

sub token {
	my $self = shift;

	my @ttl = grep defined, $self->{ttl};
	my @core = ( $self->{owner}->string, @ttl, $self->class, $self->type );

	my @rdata = $self->_empty ? () : eval { $self->_format_rdata };

	# parse into quoted strings, contiguous non-whitespace and (discarded) comments
	my @parse = map { s/\\\\/\\092/g; s/\\"/\\034/g; split /$PARSE_REGEX/o; } @rdata;
	my @token = ( @core, grep defined && length, @parse );
}


=head2 generic

    $generic = $rr->generic;

Returns the generic RR representation defined in RFC3597. This facilitates
creation of zone files containing RRs unrecognised by outdated nameservers
and provisioning software.

=cut

sub generic {
	my $self = shift;

	my @ttl = grep defined, $self->{ttl};
	my @class = map "CLASS$_", grep defined, $self->{class};
	my @core = ( $self->{owner}->string, @ttl, @class, "TYPE$self->{type}" );
	my $data = $self->rdata;
	my @data = ( '\\#', length($data), split /(\S{32})/, unpack 'H*', $data );
	my @line = _wrap( "@core (", @data, ')' );
	return join "\n\t", @line if scalar(@line) > 1;
	join ' ', @core, @data;
}


=head2 owner name

    $name = $rr->owner;

Returns the owner name of the record.

=cut

sub owner {
	my $self = shift;
	$self->{owner} = new Net::DNS::DomainName1035(shift) if scalar @_;
	$self->{owner}->name if defined wantarray;
}

sub name { &owner; }			## historical


=head2 type

    $type = $rr->type;

Returns the record type.

=cut

sub type {
	my $self = shift;
	croak 'not possible to change RR->type' if scalar @_;
	typebyval( $self->{type} );
}


=head2 class

    $class = $rr->class;

Resource record class.

=cut

sub class {
	my $self = shift;
	return $self->{class} = classbyname(shift) if scalar @_;
	defined $self->{class} ? classbyval( $self->{class} ) : 'IN';
}


=head2 ttl

    $ttl = $rr->ttl;
    $ttl = $rr->ttl(3600);

Resource record time to live in seconds.

=cut

# The following time units are recognised, but are not part of the
# published API.  These are required for parsing BIND zone files but
# should not be used in other contexts.
my %unit = ( W => 604800, D => 86400, H => 3600, M => 60, S => 1 );

sub ttl {
	my ( $self, $time ) = @_;

	return $self->{ttl} || 0 unless defined $time;		# avoid defining rr->{ttl}

	my $ttl = 0;
	my %time = reverse split /(\D)\D*/, $time . 'S';
	while ( my ( $u, $t ) = each %time ) {
		my $scale = $unit{uc $u} || die qq(bad time: $t$u);
		$ttl += $t * $scale;
	}
	$self->{ttl} = $ttl;
}


################################################################################
##
##	Default implementation for unknown RR type
##
################################################################################

sub _decode_rdata {			## decode rdata from wire-format octet string
	my ( $self, $data, $offset ) = @_;
	$self->{rdata} = substr $$data, $offset, $self->{rdlength};
}


sub _encode_rdata {			## encode rdata as wire-format octet string
	my $rdata = shift->{rdata};
}


sub _format_rdata {			## format rdata portion of RR string
	my $data = shift->rdata;
	my $size = length($data);				# RFC3597 unknown RR format
	my @data = ( '\\#', $size, split /(\S{32})/, unpack 'H*', $data );
}


sub _parse_rdata {			## parse RR attributes in argument list
	my $self = shift;
	die join ' ', 'type', $self->type, 'not implemented' if ref($self) eq __PACKAGE__;
	die join ' ', 'no zone file representation defined for', $self->type;
}


sub _defaults { }			## set attribute default values


sub dump {				## print internal data structure
	require Data::Dumper;					# uncoverable pod
	local $Data::Dumper::Maxdepth = $Data::Dumper::Maxdepth || 6;
	local $Data::Dumper::Sortkeys = $Data::Dumper::Sortkeys || 1;
	print Data::Dumper::Dumper(@_);
}

sub rdatastr {				## historical RR subtype method
	&rdstring;						# uncoverable pod
}


=head2 rdata

    $rr = new Net::DNS::RR( type => NULL, rdata => 'arbitrary' );

Resource record data section when viewed as opaque octets.

=cut

sub rdata {
	my $self = shift;

	return $self->_empty ? '' : eval { $self->_encode_rdata( 0x4000, {} ) } unless @_;

	my $data = shift || '';
	my $hash = {};
	$self->_decode_rdata( \$data, 0, $hash ) if ( $self->{rdlength} = length $data );
	croak 'unexpected compression pointer in rdata' if keys %$hash;
}


=head2 rdstring

    $rdstring = $rr->rdstring;

Returns a string representation of the RR-specific data.

=cut

sub rdstring {
	my $self = shift;

	my @rdata = $self->_empty ? () : eval { $self->_format_rdata };
	carp $@ if $@;

	join "\n\t", _wrap(@rdata);
}


=head2 rdlength

    $rdlength = $rr->rdlength;

Returns the uncompressed length of the encoded RR-specific data.

=cut

sub rdlength {
	length shift->rdata;
}


###################################################################################

=head1 Sorting of RR arrays

Sorting of RR arrays is done by Net::DNS::rrsort(), see documentation
for L<Net::DNS>. This package provides class methods to set the
comparator function used for a particular RR based on its attributes.


=head2 set_rrsort_func

    my $function = sub {		## numerically ascending order
	$Net::DNS::a->{'preference'} <=> $Net::DNS::b->{'preference'};
    };

    Net::DNS::RR::MX->set_rrsort_func( 'preference', $function );

    Net::DNS::RR::MX->set_rrsort_func( 'default_sort', $function );

set_rrsort_func() must be called as a class method. The first argument is
the attribute name on which the sorting is to take place. If you specify
"default_sort" then that is the sort algorithm that will be used when
get_rrsort_func() is called without an RR attribute as argument.

The second argument is a reference to a comparator function that uses the
global variables $a and $b in the Net::DNS package. During sorting, the
variables $a and $b will contain references to objects of the class whose
set_rrsort_func() was called. The above sorting function will only be
applied to Net::DNS::RR::MX objects.

The above example is the sorting function implemented in MX.

=cut

our %rrsortfunct;

sub set_rrsort_func {
	my $class     = shift;
	my $attribute = shift;
	my $function  = shift;

	my ($type) = $class =~ m/::([^:]+)$/;
	$rrsortfunct{$type}{$attribute} = $function;
}


=head2 get_rrsort_func

    $function = Net::DNS::RR::MX->get_rrsort_func('preference');
    $function = Net::DNS::RR::MX->get_rrsort_func();

get_rrsort_func() returns a reference to the comparator function.

=cut

my $default = sub { $Net::DNS::a->canonical() cmp $Net::DNS::b->canonical(); };

sub get_rrsort_func {
	my $class = shift;
	my $attribute = shift || 'default_sort';

	my ($type) = $class =~ m/::([^:]+)$/;

	$rrsortfunct{$type}{$attribute} || $default;
}


################################################################################
#
#  Net::DNS::RR->_subclass($rrname)
#  Net::DNS::RR->_subclass($rrname, $default)
#
# Create a new object blessed into appropriate RR subclass, after
# loading the subclass module (if necessary).  A subclass with no
# corresponding module will be regarded as unknown and blessed
# into the RR base class.
#
# The optional second argument indicates that default values are
# to be copied into the newly created object.

our %_MINIMAL = ( 255 => bless ['type' => 255], __PACKAGE__ );
our %_LOADED = %_MINIMAL;

sub _subclass {
	my ( $class, $rrname, $default ) = @_;

	unless ( $_LOADED{$rrname} ) {
		my $rrtype = typebyname($rrname);

		unless ( $_LOADED{$rrtype} ) {			# load once only
			local @INC = LIB;

			my $identifier = typebyval($rrtype);
			$identifier =~ s/\W/_/g;		# kosher Perl identifier

			my $subclass = join '::', __PACKAGE__, $identifier;

			unless ( eval "require $subclass" ) {
				push @INC, sub {
					Net::DNS::Parameters::_typespec("$rrtype.RRTYPE");
				};

				$subclass = join '::', __PACKAGE__, "TYPE$rrtype";
				eval "require $subclass";
			}

			$subclass = __PACKAGE__ if $@;

			# cache pre-built minimal and populated default object images
			my @base = ( 'type' => $rrtype );
			$_MINIMAL{$rrtype} = bless [@base], $subclass;

			my $object = bless {@base}, $subclass;
			$object->_defaults;
			$_LOADED{$rrtype} = bless [%$object], $subclass;
		}

		$_MINIMAL{$rrname} = $_MINIMAL{$rrtype};
		$_LOADED{$rrname}  = $_LOADED{$rrtype};
	}

	my $prebuilt = $default ? $_LOADED{$rrname} : $_MINIMAL{$rrname};
	bless {@$prebuilt}, ref($prebuilt);			# create object
}


sub _annotation {
	my $self = shift;
	$self->{annotation} = ["@_"] if scalar @_;
	return @{$self->{annotation} || []} if wantarray;
}


my %ignore = map( ( $_ => 1 ), @core, 'annotation', '#' );

sub _empty {
	not( $_[0]->{'#'} ||= scalar grep !$ignore{$_}, keys %{$_[0]} );
}


sub _wrap {
	my @text = @_;
	my $cols = 80;
	my $coln = 0;

	my ( @line, @fill );
	foreach (@text) {
		if ( ( $coln += 1 + length ) > $cols ) {	# start new line
			push @line, join ' ', @fill if scalar @fill;
			$coln = length;
			@fill = ();
		}
		$coln = $cols if chomp;				# force line break
		push( @fill, $_ );
	}
	push @line, join ' ', @fill;
	return @line;
}


################################################################################

our $AUTOLOAD;

sub DESTROY { }				## Avoid tickling AUTOLOAD (in cleanup)

sub AUTOLOAD {				## Default method
	my $self = shift;
	my $oref = ref($self);

	no strict q/refs/;
	my ($method) = reverse split /::/, $AUTOLOAD;
	*{$AUTOLOAD} = sub {undef};	## suppress repetition and deep recursion
	croak qq[$self has no class method "$method"] unless $oref;

	my $string = $self->string;
	my @object = grep defined($_), $oref, $oref->VERSION;
	my $module = join '::', __PACKAGE__, $self->type;
	eval("require $module") if $oref eq __PACKAGE__;

	@_ = ( <<"END", $@, "@object" );
***  FATAL PROGRAM ERROR!!	Unknown instance method "$method"
***  which the program has attempted to call for the object:
***
$string
***
***  THIS IS A BUG IN THE CALLING SOFTWARE, which incorrectly assumes
***  that the object would be of a particular type.  The type of an
***  object should be checked before calling any of its methods.
***
END
	goto &{'Carp::confess'};
}


1;
__END__


=head1 COPYRIGHT

Copyright (c)1997-2001 Michael Fuhr. 

Portions Copyright (c)2002,2003 Chris Reinhardt.

Portions Copyright (c)2005-2007 Olaf Kolkman.

Portions Copyright (c)2007,2012 Dick Franks.

All rights reserved.


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

L<perl>, L<Net::DNS>, L<Net::DNS::Question>,
L<Net::DNS::Packet>, L<Net::DNS::Update>,
RFC1035 Section 4.1.3, RFC1123, RFC3597

=cut

