package App::local::lib::Win32Helper; ## no critic(Capitalization)

=head1 NAME

App::local::lib::Win32Helper - Helper for Win32 users of local::lib.

=head1 VERSION

Version 0.990

=cut

use 5.008001;
use strict;
use warnings;

our $VERSION = '0.990';
$VERSION =~ s/_//msx;

=head1 SYNOPSIS

This module is a placeholder for the llwin32helper script, which saves the
environment variables that local::lib requires for its use to the Windows
registry.

To run it, just type

    llw32helper
    
at the command prompt.

There are no command line parameters.

=head1 CONFIGURATION AND ENVIRONMENT

The default directory to create a L<local::lib|local::lib> library in is 
determined by $ENV{HOME} if that is given, or the user's default directory
if that is not defined.

The script saves and retrieves information using the Windows registry.

=head1 DEPENDENCIES

This script depends on Perl 5.8.1 (because L<local::lib|local::lib> depends 
on it) and also depends on C<local::lib> version 1.004007, 
L<IO::Interactive|IO::Interactive> 0.0.5, L<File::HomeDir|File::HomeDir> 
0.81, L<Win32::TieRegistry|Win32::TieRegistry> 0.26, and
L<File::Spec|File::Spec> 3.2701.

=head1 BUGS AND LIMITATIONS

Please report any bugs or feature requests to 
C<< <bug-App-local-lib-Win32Helper at rt.cpan.org> >>, or through the web 
interface at L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=local-lib-Win32>. 
I will be notified, and then you'll automatically be notified of progress on 
your bug as I make changes.

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc App::local::lib::Win32Helper

You can also look for information at:

=over 4

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=App-local-lib-Win32Helper>

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/App-local-lib-Win32Helper>

=item * CPAN Ratings

L<http://cpanratings.perl.org/d/App-local-lib-Win32Helper>

=item * Search CPAN

L<http://search.cpan.org/dist/App-local-lib-Win32Helper/>

=back

=head1 AUTHOR

Curtis Jewell, C<< <csjewell at cpan.org> >>

=head1 LICENSE AND COPYRIGHT

Copyright 2010 Curtis Jewell.

This program is free software; you can redistribute it and/or modify it
under the terms of either: the GNU General Public License as published
by the Free Software Foundation; or the Artistic License.

See http://dev.perl.org/licenses/ for more information.


=cut

1;                                     # End of App::local::lib::Win32Helper
