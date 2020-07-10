package Net::DNS::Resolver::cygwin;

#
# $Id: cygwin.pm 1719 2018-11-04 05:01:43Z willem $
#
our $VERSION = (qw$LastChangedRevision: 1719 $)[1];


=head1 NAME

Net::DNS::Resolver::cygwin - Cygwin resolver class

=cut


use strict;
use warnings;
use base qw(Net::DNS::Resolver::Base);


sub _getregkey {
	my $key = join '/', @_;

	my $filehandle;
	open( $filehandle, '<', $key ) or return '';
	my $value = <$filehandle>;
	$value =~ s/\0+$// if $value;
	close($filehandle);

	return $value || '';
}


sub _init {
	my $defaults = shift->_defaults;

	my $dirhandle;

	my $root = '/proc/registry/HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Services/Tcpip/Parameters';

	unless ( -d $root ) {

		# Doesn't exist, maybe we are on 95/98/Me?
		$root = '/proc/registry/HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Services/VxD/MSTCP';
		-d $root || Carp::croak "can't read registry: $!";
	}

	# Best effort to find a useful domain name for the current host
	# if domain ends up blank, we're probably (?) not connected anywhere
	# a DNS server is interesting either...
	my $domain = _getregkey( $root, 'Domain' ) || _getregkey( $root, 'DhcpDomain' );

	# If nothing else, the searchlist should probably contain our own domain
	# also see below for domain name devolution if so configured
	# (also remove any duplicates later)
	my $devolution = _getregkey( $root, 'UseDomainNameDevolution' );
	my $searchlist = _getregkey( $root, 'SearchList' );
	my @searchlist = ( $domain, split m/[\s,]+/, $searchlist );


	# This is (probably) adequate on NT4
	my @nt4nameservers;
	foreach ( grep length, _getregkey( $root, 'NameServer' ), _getregkey( $root, 'DhcpNameServer' ) ) {
		push @nt4nameservers, split m/[\s,]+/;
		last;
	}


	# but on W2K/XP the registry layout is more advanced due to dynamically
	# appearing connections. So we attempt to handle them, too...
	# opt to silently fail if something isn't ok (maybe we're on NT4)
	# If this doesn't fail override any NT4 style result we found, as it
	# may be there but is not valid.
	# drop any duplicates later
	my @nameservers;

	my $dnsadapters = join '/', $root, 'DNSRegisteredAdapters';
	if ( opendir( $dirhandle, $dnsadapters ) ) {
		my @adapters = grep !/^\.\.?$/, readdir($dirhandle);
		closedir($dirhandle);
		foreach my $adapter (@adapters) {
			my $ns = _getregkey( $dnsadapters, $adapter, 'DNSServerAddresses' );
			until ( length($ns) < 4 ) {
				push @nameservers, join '.', unpack( 'C4', $ns );
				substr( $ns, 0, 4 ) = '';
			}
		}
	}

	my $interfaces = join '/', $root, 'Interfaces';
	if ( opendir( $dirhandle, $interfaces ) ) {
		my @ifacelist = grep !/^\.\.?$/, readdir($dirhandle);
		closedir($dirhandle);
		foreach my $iface (@ifacelist) {
			my $ip = _getregkey( $interfaces, $iface, 'DhcpIPAddress' )
					|| _getregkey( $interfaces, $iface, 'IPAddress' );
			next unless $ip;
			next if $ip eq '0.0.0.0';

			foreach (
				grep length,
				_getregkey( $interfaces, $iface, 'NameServer' ),
				_getregkey( $interfaces, $iface, 'DhcpNameServer' )
				) {
				push @nameservers, split m/[\s,]+/;
				last;
			}
		}
	}

	@nameservers = @nt4nameservers unless @nameservers;
	$defaults->nameservers(@nameservers);


	# fix devolution if configured, and simultaneously
	# eliminate duplicate entries (but keep the order)
	my @list;
	my %seen;
	foreach (@searchlist) {
		s/\.+$//;
		push( @list, $_ ) unless $seen{lc $_}++;

		next unless $devolution;

		# while there are more than two labels, cut
		while (s#^[^.]+\.(.+\..+)$#$1#) {
			push( @list, $_ ) unless $seen{lc $_}++;
		}
	}
	$defaults->searchlist(@list);

	%$defaults = Net::DNS::Resolver::Base::_untaint(%$defaults);

	$defaults->_read_env;
}


1;
__END__


=head1 SYNOPSIS

    use Net::DNS::Resolver;

=head1 DESCRIPTION

This class implements the OS specific portions of C<Net::DNS::Resolver>.

No user serviceable parts inside, see L<Net::DNS::Resolver>
for all your resolving needs.

=head1 COPYRIGHT

Copyright (c)2003 Sidney Markowitz.

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

L<perl>, L<Net::DNS>, L<Net::DNS::Resolver>

=cut

