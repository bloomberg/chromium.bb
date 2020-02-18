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

use 5.00503;
use strict;

# On some platforms (mostly Windows), we get errors because
# of Term::Cap issues. To avoid this, set TERM=dumb if the
# user does not have a TERM value already.
# This doesn't remove all possible errors, just the most
# annoying and common ones.
BEGIN {
	$ENV{TERM} ||= 'dumb';
}

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.06';
}

use pler ();

unless ( $VERSION eq $pler::VERSION ) {
	die "Error: Version mismatch (launch script $VERSION using module $pler::VERSION)";
}
pler::main(@ARGV);

exit(0);

__END__
:endofperl
