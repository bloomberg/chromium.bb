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

use 5.006;
use strict;
use pip;

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.19';
}

eval {
	pip::main( @ARGV );
};

if ( $@ ) {
	my $errstr = $@;
	$errstr =~ s/\sat line\b.+$//;
	print "Unexpected Error: $errstr\n";
	exit(255);
}

exit(0);

__END__
:endofperl
