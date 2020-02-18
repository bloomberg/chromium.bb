package Crypt::RSA::Debug; 
use strict;
use warnings;

## Crypt::RSA::Debug
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.

use vars qw(@ISA @EXPORT_OK);
require Exporter;
@ISA = qw(Exporter);

@EXPORT_OK = qw(debug debuglevel); 

my $DEBUG = 0; 

sub debug{
    return unless $DEBUG;
    my ($caller, undef) = caller;
    my (undef,undef,$line,$sub) = caller(1); $sub =~ s/.*://;
    $sub = sprintf "%12s()%4d", $sub, $line;
    $sub .= " |  " . (shift);  
    $sub =~ s/\x00/[0]/g; 
    $sub =~ s/\x01/[1]/g; 
    $sub =~ s/\x02/[2]/g; 
    $sub =~ s/\x04/[4]/g; 
    $sub =~ s/\x05/[5]/g; 
    $sub =~ s/\xff/[-]/g; 
    $sub =~ s/[\x00-\x1f]/\./g; 
    $sub =~ s/[\x7f-\xfe]/_/g;
    print "$sub\n";
}


sub debuglevel { 

    my ($level) = shift;
    $DEBUG = $level;

}


=head1 NAME

Crypt::RSA::Debug - Debug routine for Crypt::RSA.

=head1 SYNOPSIS

    use Crypt::RSA::Debug qw(debug);
    debug ("oops!");

=head1 DESCRIPTION

The module provides support for the I<print> method of debugging!

=head1 FUNCTION 

=over 4

=item B<debug> String

Prints B<String> on STDOUT, along with caller's function name and line number.

=item B<debuglevel> Integer

Sets the class data I<debuglevel> to specified value. The value
defaults to 0. Callers can use the debuglevel facility by
comparing $Crypt::RSA::DEBUG to the desired debug level before
generating a debug statement.

=back

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=cut

1;

