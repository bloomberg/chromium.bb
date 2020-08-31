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
#!perl
#line 29
#!/usr/bin/bash

USAGE=<<...
Usage:

  test-yaml <test> <options>

Tests:

  load                  # Load YAML input and write data output
  dump                  # Load data input and write YAML output
  yny                   # Load→Dump→Load roundtrip
  nyn                   # Dump→Load→Dump roundtrip
  parse                 # Parse YAML input and write YAML events
  emit                  # Emit YAML events and write YAML output

Options:

  --in=<file>           # Input file
  --out=<file>          # Output file
  --from=<format>       # Input format
  --to=<format>         # Output format
  --yaml=<framework>    # Which YAML implementations to use
  --test=<tag-spec>     # Tags of test cases to use

Formats:
  json                  # JSON input or output
  perl                  # Perl code producing or representing data

Examples:

  test-yaml load --in=file.yaml --to=json --yaml=perl
  test-yaml dump --in=prog.pl --tag=dump --yaml=perl

See:

  For more complete doc, run:

    > man test-yaml

...

__END__
:endofperl
