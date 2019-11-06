@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
IF EXIST "%~dp0perl.exe" (
"%~dp0perl.exe" -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
) ELSE IF EXIST "%~dp0..\..\bin\perl.exe" (
"%~dp0..\..\bin\perl.exe" -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
) ELSE (
perl -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
)

goto endofperl
:WinNT
IF EXIST "%~dp0perl.exe" (
"%~dp0perl.exe" -x -S %0 %*
) ELSE IF EXIST "%~dp0..\..\bin\perl.exe" (
"%~dp0..\..\bin\perl.exe" -x -S %0 %*
) ELSE (
perl -x -S %0 %*
)

if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl
@rem ';
#!/usr/bin/perl
#line 29

use strict;
use warnings;
# PODNAME: package-stash-conflicts

# this script was generated with Dist::Zilla::Plugin::Conflicts 0.19

use Getopt::Long;
use Package::Stash::Conflicts;

my $verbose;
GetOptions( 'verbose|v' => \$verbose );

if ($verbose) {
    Package::Stash::Conflicts->check_conflicts;
}
else {
    my @conflicts = Package::Stash::Conflicts->calculate_conflicts;
    print "$_\n" for map { $_->{package} } @conflicts;
    exit @conflicts;
}

__END__

=pod

=encoding UTF-8

=head1 NAME

package-stash-conflicts

=head1 VERSION

version 0.38

=head1 SUPPORT

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=Package-Stash>
(or L<bug-Package-Stash@rt.cpan.org|mailto:bug-Package-Stash@rt.cpan.org>).

=head1 AUTHOR

Jesse Luehrs <doy@tozt.net>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018 by Jesse Luehrs.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

__END__
:endofperl
