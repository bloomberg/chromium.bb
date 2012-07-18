package Test::Output::Tie;
use vars qw($VERSION);

$VERSION='1.01';

use strict;
use warnings;

=head1 NAME

Test::Output::Tie - module used by Test::Output to tie STDERR and STDOUT

=head1 DESCRIPTION

You are probably more interested in reading Test::Output.

This module is used to tie STDOUT and STDERR in Test::Output.

=cut

=head2 Methods

=over 4

=item TIEHANDLE

The constructor for the class.

=cut

sub TIEHANDLE {
  my $class = shift;
  my $scalar = '';
  my $obj = shift || \$scalar; 

  bless( $obj, $class);
}

=item PRINT

This method is called each time STDERR or STDOUT are printed to.

=cut

sub PRINT {
    my $self = shift;
    $$self .= join(defined $, ? $, : '', @_);
    $$self .= defined $\ ? $\ : ''; # for say()
}

=item PRINTF

This method is called each time STDERR or STDOUT are printed to with C<printf>.

=cut

sub PRINTF {
    my $self = shift;
    my $fmt  = shift;
    $$self .= sprintf $fmt, @_;
}

=item FILENO

=cut

sub FILENO {}

=item BINMODE

=cut

sub BINMODE {}

=item read

This function is used to return all output printed to STDOUT or STDERR.

=cut

sub read {
    my $self = shift;
    my $data = $$self;
    $$self = '';
    return $data;
}

=back

=head1 AUTHOR

Currently maintained by brian d foy, C<bdfoy@cpan.org>.

Shawn Sorichetti, C<< <ssoriche@cpan.org> >>

=head1 SOURCE AVAILABILITY
 
This module is in Github:

	http://github.com/briandfoy/test-output/tree/master
	
=head1 COPYRIGHT & LICENSE

Currently maintained by brian d foy, C<bdfoy@cpan.org>.

Copyright 2005-2008 Shawn Sorichetti, All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.


=head1 ACKNOWLEDGMENTS

This code was taken from Test::Simple's TieOut.pm maintained 
Michael G Schwern. TieOut.pm was originally written by chromatic.

Thanks for the idea and use of the code.

=cut

1;
