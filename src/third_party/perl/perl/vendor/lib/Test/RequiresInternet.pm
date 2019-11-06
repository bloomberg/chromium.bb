use strict;
use warnings;
package Test::RequiresInternet;
$Test::RequiresInternet::VERSION = '0.05';
# ABSTRACT: Easily test network connectivity


use Socket;

sub import {
    skip_all("NO_NETWORK_TESTING") if env("NO_NETWORK_TESTING");

    my $namespace = shift;

    my $argc = scalar @_;
    if ( $argc == 0 ) {
        push @_, 'www.google.com', 80;
    }
    elsif ( $argc % 2 != 0 ) {
        die "Must supply server and a port pairs. You supplied " . (join ", ", @_) . "\n";
    }

    while ( @_ ) {
        my $host = shift;
        my $port = shift;

        local $@;

        eval {make_socket($host, $port)};

        if ( $@ ) {
            skip_all("$@");
        }
    }
}

sub make_socket {
    my ($host, $port) = @_;

    my $portnum;
    if ($port =~ /\D/) { 
        $portnum = getservbyname($port, "tcp");
    }
    else {
        $portnum = $port;
    }

    die "Could not find a port number for $port\n" if not $portnum;

    my $iaddr = inet_aton($host) or die "no host: $host\n";

    my $paddr = sockaddr_in($portnum, $iaddr);
    my $proto = getprotobyname("tcp");

    socket(my $sock, PF_INET, SOCK_STREAM, $proto) or die "socket: $!\n";
    connect($sock, $paddr) or die "connect: $!\n";
    close ($sock) or die "close: $!\n";

    1;
}

sub env {
    exists $ENV{$_[0]} && $ENV{$_[0]} eq '1'
}

sub skip_all {
    my $reason = shift;
    print "1..0 # Skipped: $reason";
    exit 0;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::RequiresInternet - Easily test network connectivity

=head1 VERSION

version 0.05

=head1 SYNOPSIS

  use Test::More;
  use Test::RequiresInternet ('www.example.com' => 80, 'foobar.io' => 25);

  # if you reach here, sockets successfully connected to hosts/ports above
  plan tests => 1;

  ok(do_that_internet_thing());

=head1 OVERVIEW

This module is intended to easily test network connectivity before functional 
tests begin to non-local Internet resources.  It does not require any modules
beyond those supplied in core Perl.

If you do not specify a host/port pair, then the module defaults to using
C<www.google.com> on port C<80>.  

You may optionally specify the port by its name, as in C<http> or C<ldap>.
If you do this, the test module will attempt to look up the port number
using C<getservbyname>.

If you do specify a host and port, they must be specified in B<pairs>. It is a
fatal error to omit one or the other.

If the environment variable C<NO_NETWORK_TESTING> is set, then the tests
will be skipped without attempting any socket connections.

If the sockets cannot connect to the specified hosts and ports, the exception
is caught, reported and the tests skipped.

=head1 AUTHOR

Mark Allen <mrallen1@yahoo.com>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2014 by Mark Allen.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
