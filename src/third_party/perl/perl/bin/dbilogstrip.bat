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

=head1 NAME

dbilogstrip - filter to normalize DBI trace logs for diff'ing

=head1 SYNOPSIS

Read DBI trace file C<dbitrace.log> and write out a stripped version to C<dbitrace_stripped.log>

  dbilogstrip dbitrace.log > dbitrace_stripped.log

Run C<yourscript.pl> twice, each with different sets of arguments, with
DBI_TRACE enabled. Filter the output and trace through C<dbilogstrip> into a
separate file for each run. Then compare using diff. (This example assumes
you're using a standard shell.)

  DBI_TRACE=2 perl yourscript.pl ...args1... 2>&1 | dbilogstrip > dbitrace1.log
  DBI_TRACE=2 perl yourscript.pl ...args2... 2>&1 | dbilogstrip > dbitrace2.log
  diff -u dbitrace1.log dbitrace2.log

=head1 DESCRIPTION

Replaces any hex addresses, e.g, C<0x128f72ce> with C<0xN>.

Replaces any references to process id or thread id, like C<pid#6254> with C<pidN>.

So a DBI trace line like this:

  -> STORE for DBD::DBM::st (DBI::st=HASH(0x19162a0)~0x191f9c8 'f_params' ARRAY(0x1922018)) thr#1800400

will look like this:

  -> STORE for DBD::DBM::st (DBI::st=HASH(0xN)~0xN 'f_params' ARRAY(0xN)) thrN

=cut

use strict;

while (<>) {
    # normalize hex addresses: 0xDEADHEAD => 0xN
    s/ \b 0x [0-9a-f]+ /0xN/gx;
    # normalize process and thread id number
    s/ \b (pid|tid|thr) \W? \d+ /${1}N/gx;

} continue {
    print or die "-p destination: $!\n";
}



__END__
:endofperl
