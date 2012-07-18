@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
"%~dp0perl.exe" -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofperl
:WinNT
"%~dp0perl.exe" -x -S %0 %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl
@rem ';
#!/usr/bin/perl
#line 15

use strict;
use warnings;
# PODNAME: package-stash-conflicts

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
:endofperl
