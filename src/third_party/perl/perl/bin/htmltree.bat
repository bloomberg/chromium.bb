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
# Time-stamp: "2000-10-02 14:48:15 MDT"
#
# Parse the given HTML file(s) and dump the parse tree
# Usage:
#  htmltree -D3 -w file1 file2 file3
#    -D[number]  sets HTML::TreeBuilder::Debug to that figure.
#    -w  turns on $tree->warn(1) for the new tree

require 5;
use warnings;
use strict;

my $warn;

BEGIN { # We have to set debug level before we use HTML::TreeBuilder.
  $HTML::TreeBuilder::DEBUG = 0; # default debug level
  $warn = 0;
  while(@ARGV) {   # lameo switch parsing
    if($ARGV[0] =~ m<^-D(\d+)$>s) {
      $HTML::TreeBuilder::DEBUG = $1;
      print "Debug level $HTML::TreeBuilder::DEBUG\n";
      shift @ARGV;
    } elsif ($ARGV[0] =~ m<^-w$>s) {
      $warn = 1;
      shift @ARGV;
    } else {
      last;
    }
  }
}

use HTML::TreeBuilder;

foreach my $file (grep( -f $_, @ARGV)) {
  print
    "=" x 78, "\n",
    "Parsing $file...\n";

  my $h = HTML::TreeBuilder->new;
  $h->ignore_unknown(0);
  $h->warn($warn);
  $h->parse_file($file);

  print "- "x 39, "\n";
  $h->dump();
  $h = $h->delete(); # nuke it!
  print "\n\n";
}
exit;
__END__

__END__
:endofperl
