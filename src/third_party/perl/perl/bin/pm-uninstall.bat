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
#!C:\strawberry\perl\bin\perl.exe 
#line 29

use strict;
use warnings;
use App::pmuninstall;

App::pmuninstall->new->run(@ARGV);

__END__

=head1 NAME

  pm-uninstall - Uninstall modules

=head1 SYNOPSIS

  pm-uninstall [options] Module ...

  options:
      -v,--verbose                  Turns on chatty output
      -f,--force                    Uninstalls without prompts
      -c,--checkdeps                Check dependencies (defaults to on)
      -n,--no-checkdeps             Don't check dependencies
      -q,--quiet                    Suppress some messages
      -h,--help                     This help message
      -V,--version                  Show version
      -l,--local-lib                Additional module path
      -L,--local-lib-contained      Additional module path (don't include non-core modules)

=cut

__END__
:endofperl
