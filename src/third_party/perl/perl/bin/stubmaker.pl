#!/usr/bin/env perl 
#!d:\perl\bin\perl.exe 
#
# Filename: stubmaker.pl
# Authors: Byrne Reese <byrne at majordojo dot com>
#          Paul Kulchenko
#
# Copyright (C) 2005 Byrne Reese
#
# Usage:
#    stubmaker.pl -[vd] <WSDL URL>
###################################################

use SOAP::Lite;
use Getopt::Long;

my $VERBOSE = 0;
my $DIRECTORY = ".";
GetOptions(
	   'd=s' => \$DIRECTORY,
	   'v' => \$VERBOSE,
	   help => sub { HELP_MESSAGE(); },
	   version => sub { VERSION_MESSAGE(); exit(0); },
	   ) or HELP_MESSAGE();

HELP_MESSAGE() unless $ARGV[0];

my $WSDL_URL = shift;

print "Writing stub files...\n" if $VERBOSE;
my %services = %{SOAP::Schema->schema_url($WSDL_URL)
                             ->cache_ttl(1)
                             ->cache_dir($DIRECTORY)
                             ->parse()
                             ->load
                             ->services};
Carp::croak "More than one service in service description. Service and port names have to be specified\n" 
    if keys %services > 1; 

sub VERSION_MESSAGE {
    print "$0 $SOAP::Lite::VERSION (C) 2005 Byrne Reese.\n";
}

sub HELP_MESSAGE {
    VERSION_MESSAGE();
    print <<EOT;
usage: $0 -[options] <WSDL URL>
options:
  -v             Verbose Outputbe quiet
  -d <dirname>   Output directory
EOT
exit 0;
}

__END__

=pod

=head1 NAME

stubmaker.pl - Generates client stubs from a WSDL file.

=head1 OPTIONS

=over

=item -d <dirname>

Specifies the directory you wish to output the files to. The directory must already exist.

=item -v

Turns on "verbose" output during the code stub generation process. To be honest, there is not much the program outputs, but if you must see something output to the console, then this fits the bill.

=item --help

Outputs a short help message.

=item --version

Outputs the current version of stubmaker.pl.

=back

=cut

=head1 STUB FILES

=head2 STUB SUBROUTINES

The "class" or "package" created by stubmaker.pl is actually a sub-class of
the core SOAP::Lite object. As a result, all methods one can call upon 
L<SOAP::Lite> one can also call upon generated stubs.

For example, suppose you wanted to obtain readable output from the generated
stub, then simply call C<readable(1)> on the stub's instance. See the example
below.

The following subroutines are unique to generated stub classes, and help the
user control and configure the stub class.

=over

=item want_som(boolean)

When set to 1, SOAP::Lite will return SOAP::SOM objects to the user upon
invoking a method on a remote service. This is very helpful when you need
to check to see if the return value is a SOAP::Fault or not. When set to 0,
SOAP::Lite will return the return value of the method.

=back

=cut

=head1 EXAMPLES

=head2 Invoking stubmaker.pl from the command line

> perl stubmaker.pl http://www.xmethods.net/sd/StockQuoteService.wsdl
Or:
> perl "-MStockQuoteService qw(:all)" -le "print getQuote('MSFT')" 

=head2 Working with stub classes

Command line:
> perl stubmaker.pl http://ws1.api.re2.yahoo.com/ws/soap-demo/full.wsdl

File: echo.pl
> use full;
> use SOAP::Lite +trace => qw( debug );
> my $f = new full;
> $f->use_prefix(0);
> $f->readable(1);
> $f->want_som(1);
> $som = $f->echoViaBase64("foo");

=head1 COPYRIGHT

Copyright (C) 2000-2005 Paul Kulchenko. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.
