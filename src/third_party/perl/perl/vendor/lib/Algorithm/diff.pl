#!/usr/bin/perl
#
# `Diff' program in Perl
# Copyright 1998 M-J. Dominus. (mjd-perl-diff@plover.com)
#
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#

use Algorithm::Diff qw(diff);

bag("Usage: $0 oldfile newfile") unless @ARGV == 2;

my ($file1, $file2) = @ARGV;

# -f $file1 or bag("$file1: not a regular file");
# -f $file2 or bag("$file2: not a regular file");

-T $file1 or bag("$file1: binary");
-T $file2 or bag("$file2: binary");

open (F1, $file1) or bag("Couldn't open $file1: $!");
open (F2, $file2) or bag("Couldn't open $file2: $!");
chomp(@f1 = <F1>);
close F1;
chomp(@f2 = <F2>);
close F2;

$diffs = diff(\@f1, \@f2);
exit 0 unless @$diffs;

foreach $chunk (@$diffs) {
  
  foreach $line (@$chunk) {
    my ($sign, $lineno, $text) = @$line;
    printf "%4d$sign %s\n", $lineno+1, $text;
  }
  print "--------\n";
}
exit 1;

sub bag {
  my $msg = shift;
  $msg .= "\n";
  warn $msg;
  exit 2;
}
