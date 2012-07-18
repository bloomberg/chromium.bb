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
#!perl
#line 15
use strict;
BEGIN {
my $old = select STDERR; $|++;  # turn on autoflush
select $old;             $|++;  # turn on autoflush
$0 = shift(@ARGV);              # rename the script
my $rv = do($0);                # execute the file
die $@ if $@;                   # die on parse/execute error
}
### XXX 'do' returns last statement evaluated, which may be
### undef as well. So don't die in that case.
#die $! if not defined $rv;      # die on execute error

__END__
:endofperl
