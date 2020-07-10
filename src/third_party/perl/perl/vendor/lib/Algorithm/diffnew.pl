#!/usr/bin/perl
#
# `Diff' program in Perl
# Copyright 1998 M-J. Dominus. (mjd-perl-diff@plover.com)
#
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# Altered to output in `context diff' format (but without context)
# September 1998 Christian Murphy (cpm@muc.de)
#
# Context lines feature added
# Unified, "Old" (Standard UNIX), Ed diff added September 1998
# Reverse_Ed (-f option) added March 1999
# Amir D. Karger (karger@bead.aecom.yu.edu)
#
# Modular functions integrated into program
# February 1999 M-J. Dominus (mjd-perl-diff@plover.com)
#
# In this file, "item" usually means "line of text", and "item number" usually
# means "line number". But theoretically the code could be used more generally
use strict;
use Algorithm::Diff qw(diff);

# GLOBAL VARIABLES  ####
# After we've read up to a certain point in each file, the number of items
# we've read from each file will differ by $FLD (could be 0)
my $File_Length_Difference = 0;

#ed diff outputs hunks *backwards*, so we need to save hunks when doing ed diff
my @Ed_Hunks = ();
########################

my $usage = << "ENDUSAGE";
Usage: $0 [{-c | -C lines -e | -f | -u | -U lines |-q | -i | -w}] oldfile newfile
    -c do a context diff with 3 lines of context
    -C do a context diff with 'lines' lines of context (implies -c)
    -e create a script for the ed editor to change oldfile to newfile
    -f like -e but in reverse order
    -u do a unified diff with 3 lines of context
    -U do a unified diff with 'lines' lines of context (implies -u)
    -q report only whether or not the files differ
    -i ignore differences in Upper/lower-case
    -w ignore differences in white-space (space and TAB characters)

By default it will do an "old-style" diff, with output like UNIX diff
ENDUSAGE

my $Context_Lines = 0; # lines of context to print. 0 for old-style diff
my $Diff_Type = "OLD"; # by default, do standard UNIX diff
my ($opt_c, $opt_u, $opt_e, $opt_f, $opt_q, $opt_i, $opt_w);
my $compareRoutineRef = undef;
while ($ARGV[0] =~ /^-/) {
  my $opt = shift;
  last if $opt eq '--';
  if ($opt =~ /^-C(.*)/) {
    $Context_Lines = $1 || shift;
    $opt_c = 1;
    $Diff_Type = "CONTEXT";
  } elsif ($opt =~ /^-c$/) {
    $Context_Lines = 3;
    $opt_c = 1;
    $Diff_Type = "CONTEXT";
  } elsif ($opt =~ /^-e$/) {
    $opt_e = 1;
    $Diff_Type = "ED";
  } elsif ($opt =~ /^-f$/) {
    $opt_f = 1;
    $Diff_Type = "REVERSE_ED";
  } elsif ($opt =~ /^-U(.*)$/) {
    $Context_Lines = $1 || shift;
    $opt_u = 1;
    $Diff_Type = "UNIFIED";
  } elsif ($opt =~ /^-u$/) {
    $Context_Lines = 3;
    $opt_u = 1;
    $Diff_Type = "UNIFIED";
  } elsif ($opt =~ /^-q$/) {
    $Context_Lines = 0;
    $opt_q = 1;
    $opt_e = 1;
    $Diff_Type = "ED";
  } elsif ($opt =~ /^-i$/) {
    $opt_i = 1;
    $compareRoutineRef = \&compareRoutine;
  } elsif ($opt =~ /^-w$/) {
    $opt_w = 1;
    $compareRoutineRef = \&compareRoutine;
  } else {
    $opt =~ s/^-//;
    bag("Illegal option -- $opt");
  }
}

if ($opt_q and grep($_,($opt_c, $opt_f, $opt_u)) > 1) {
    bag("Combining -q with other options is nonsensical");
}

if (grep($_,($opt_c, $opt_e, $opt_f, $opt_u)) > 1) {
    bag("Only one of -c, -u, -f, -e are allowed");
}

bag($usage) unless @ARGV == 2;

######## DO THE DIFF!
my ($file1, $file2) = @ARGV;

my ($char1, $char2); # string to print before file names
if ($Diff_Type eq "CONTEXT") {
    $char1 = '*' x 3; $char2 = '-' x 3;
} elsif ($Diff_Type eq "UNIFIED") {
    $char1 = '-' x 3; $char2 = '+' x 3;
}

open (F1, $file1) or bag("Couldn't open $file1: $!");
open (F2, $file2) or bag("Couldn't open $file2: $!");
my (@f1, @f2);
chomp(@f1 = <F1>);
close F1;
chomp(@f2 = <F2>);
close F2;

# diff yields lots of pieces, each of which is basically a Block object
my $diffs = diff(\@f1, \@f2, $compareRoutineRef);
exit 0 unless @$diffs;

if ($opt_q and @$diffs) {
    print "Files $file1 and $file2 differ\n";
    exit 1;
}

if ($Diff_Type =~ /UNIFIED|CONTEXT/) {
    my @st = stat($file1);
    my $MTIME = 9;
    print "$char1 $file1\t", scalar localtime($st[$MTIME]), "\n";
    @st = stat($file2);
    print "$char2 $file2\t", scalar localtime($st[$MTIME]), "\n";
}

my ($hunk,$oldhunk);
# Loop over hunks. If a hunk overlaps with the last hunk, join them.
# Otherwise, print out the old one.
foreach my $piece (@$diffs) {
    $hunk = new Hunk ($piece, $Context_Lines);
    next unless $oldhunk; # first time through

    # Don't need to check for overlap if blocks have no context lines
    if ($Context_Lines && $hunk->does_overlap($oldhunk)) {
	$hunk->prepend_hunk($oldhunk);
    } else {
	$oldhunk->output_diff(\@f1, \@f2, $Diff_Type);
    }

} continue {
    $oldhunk = $hunk;
}

# print the last hunk
$oldhunk->output_diff(\@f1, \@f2, $Diff_Type);

# Print hunks backwards if we're doing an ed diff
map {$_->output_ed_diff(\@f1, \@f2, $Diff_Type)} @Ed_Hunks if @Ed_Hunks;

exit 1;
# END MAIN PROGRAM

sub bag {
  my $msg = shift;
  $msg .= "\n";
  warn $msg;
  exit 2;
}

sub compareRoutine {
  my $line = shift;
  $line =~ s/[ \t\r\n]+//g  if ($opt_w);
  $line = uc( $line )   if ($opt_i);
  return $line;
}

########
# Package Hunk. A Hunk is a group of Blocks which overlap because of the
# context surrounding each block. (So if we're not using context, every
# hunk will contain one block.)
{
package Hunk;

sub new {
# Arg1 is output from &LCS::diff (which corresponds to one Block)
# Arg2 is the number of items (lines, e.g.,) of context around each block
#
# This subroutine changes $File_Length_Difference
#
# Fields in a Hunk:
# blocks      - a list of Block objects
# start       - index in file 1 where first block of the hunk starts
# end         - index in file 1 where last block of the hunk ends
#
# Variables:
# before_diff - how much longer file 2 is than file 1 due to all hunks
#               until but NOT including this one
# after_diff  - difference due to all hunks including this one
    my ($class, $piece, $context_items) = @_;

    my $block = new Block ($piece); # this modifies $FLD!

    my $before_diff = $File_Length_Difference; # BEFORE this hunk
    my $after_diff = $before_diff + $block->{"length_diff"};
    $File_Length_Difference += $block->{"length_diff"};

    # @remove_array and @insert_array hold the items to insert and remove
    # Save the start & beginning of each array. If the array doesn't exist
    # though (e.g., we're only adding items in this block), then figure
    # out the line number based on the line number of the other file and
    # the current difference in file lenghts
    my @remove_array = $block->remove;
    my @insert_array = $block->insert;
    my ($a1, $a2, $b1, $b2, $start1, $start2, $end1, $end2);
    $a1 = @remove_array ? $remove_array[0 ]->{"item_no"} : -1;
    $a2 = @remove_array ? $remove_array[-1]->{"item_no"} : -1;
    $b1 = @insert_array ? $insert_array[0 ]->{"item_no"} : -1;
    $b2 = @insert_array ? $insert_array[-1]->{"item_no"} : -1;

    $start1 = $a1 == -1 ? $b1 - $before_diff : $a1;
    $end1   = $a2 == -1 ? $b2 - $after_diff  : $a2;
    $start2 = $b1 == -1 ? $a1 + $before_diff : $b1;
    $end2   = $b2 == -1 ? $a2 + $after_diff  : $b2;

    # At first, a hunk will have just one Block in it
    my $hunk = {
	    "start1" => $start1,
	    "start2" => $start2,
	    "end1" => $end1,
	    "end2" => $end2,
	    "blocks" => [$block],
              };
    bless $hunk, $class;

    $hunk->flag_context($context_items);

    return $hunk;
}

# Change the "start" and "end" fields to note that context should be added
# to this hunk
sub flag_context {
    my ($hunk, $context_items) = @_;
    return unless $context_items; # no context

    # add context before
    my $start1 = $hunk->{"start1"};
    my $num_added = $context_items > $start1 ? $start1 : $context_items;
    $hunk->{"start1"} -= $num_added;
    $hunk->{"start2"} -= $num_added;

    # context after
    my $end1 = $hunk->{"end1"};
    $num_added = ($end1+$context_items > $#f1) ?
                  $#f1 - $end1 :
                  $context_items;
    $hunk->{"end1"} += $num_added;
    $hunk->{"end2"} += $num_added;
}

# Is there an overlap between hunk arg0 and old hunk arg1?
# Note: if end of old hunk is one less than beginning of second, they overlap
sub does_overlap {
    my ($hunk, $oldhunk) = @_;
    return "" unless $oldhunk; # first time through, $oldhunk is empty

    # Do I actually need to test both?
    return ($hunk->{"start1"} - $oldhunk->{"end1"} <= 1 ||
            $hunk->{"start2"} - $oldhunk->{"end2"} <= 1);
}

# Prepend hunk arg1 to hunk arg0
# Note that arg1 isn't updated! Only arg0 is.
sub prepend_hunk {
    my ($hunk, $oldhunk) = @_;

    $hunk->{"start1"} = $oldhunk->{"start1"};
    $hunk->{"start2"} = $oldhunk->{"start2"};

    unshift (@{$hunk->{"blocks"}}, @{$oldhunk->{"blocks"}});
}


# DIFF OUTPUT ROUTINES. THESE ROUTINES CONTAIN DIFF FORMATTING INFO...
sub output_diff {
# First arg is the current hunk of course
# Next args are refs to the files
# last arg is type of diff
    my $diff_type = $_[-1];
    my %funchash  = ("OLD"        => \&output_old_diff,
                     "CONTEXT"    => \&output_context_diff,
		     "ED"         => \&store_ed_diff,
		     "REVERSE_ED" => \&output_ed_diff,
                     "UNIFIED"    => \&output_unified_diff,
	            );
    if (exists $funchash{$diff_type}) {
        &{$funchash{$diff_type}}(@_); # pass in all args
    } else {die "unknown diff type $diff_type"}
}

sub output_old_diff {
# Note that an old diff can't have any context. Therefore, we know that
# there's only one block in the hunk.
    my ($hunk, $fileref1, $fileref2) = @_;
    my %op_hash = ('+' => 'a', '-' => 'd', '!' => 'c');

    my @blocklist = @{$hunk->{"blocks"}};
    warn ("Expecting one block in an old diff hunk!") if scalar @blocklist != 1;
    my $block = $blocklist[0];
    my $op = $block->op; # +, -, or !

    # Calculate item number range.
    # old diff range is just like a context diff range, except the ranges
    # are on one line with the action between them.
    my $range1 = $hunk->context_range(1);
    my $range2 = $hunk->context_range(2);
    my $action = $op_hash{$op} || warn "unknown op $op";
    print "$range1$action$range2\n";

    # If removing anything, just print out all the remove lines in the hunk
    # which is just all the remove lines in the block
    if ($block->remove) {
	my @outlist = @$fileref1[$hunk->{"start1"}..$hunk->{"end1"}];
	map {$_ = "< $_\n"} @outlist; # all lines will be '< text\n'
	print @outlist;
    }

    print "---\n" if $op eq '!'; # only if inserting and removing
    if ($block->insert) {
	my @outlist = @$fileref2[$hunk->{"start2"}..$hunk->{"end2"}];
	map {$_ = "> $_\n"} @outlist; # all lines will be '> text\n'
	print @outlist;
    }
}

sub output_unified_diff {
    my ($hunk, $fileref1, $fileref2) = @_;
    my @blocklist;

    # Calculate item number range.
    my $range1 = $hunk->unified_range(1);
    my $range2 = $hunk->unified_range(2);
    print "@@ -$range1 +$range2 @@\n";

    # Outlist starts containing the hunk of file 1.
    # Removing an item just means putting a '-' in front of it.
    # Inserting an item requires getting it from file2 and splicing it in.
    #    We splice in $num_added items. Remove blocks use $num_added because
    # splicing changed the length of outlist.
    #    We remove $num_removed items. Insert blocks use $num_removed because
    # their item numbers---corresponding to positions in file *2*--- don't take
    # removed items into account.
    my $low = $hunk->{"start1"};
    my $hi = $hunk->{"end1"};
    my ($num_added, $num_removed) = (0,0);
    my @outlist = @$fileref1[$low..$hi];
    map {s/^/ /} @outlist; # assume it's just context

    foreach my $block (@{$hunk->{"blocks"}}) {
	foreach my $item ($block->remove) {
	    my $op = $item->{"sign"}; # -
	    my $offset = $item->{"item_no"} - $low + $num_added;
	    $outlist[$offset] =~ s/^ /$op/;
	    $num_removed++;
	}
	foreach my $item ($block->insert) {
	    my $op = $item->{"sign"}; # +
	    my $i = $item->{"item_no"};
	    my $offset = $i - $hunk->{"start2"} + $num_removed;
	    splice(@outlist,$offset,0,"$op$$fileref2[$i]");
	    $num_added++;
	}
    }

    map {s/$/\n/} @outlist; # add \n's
    print @outlist;

}

sub output_context_diff {
    my ($hunk, $fileref1, $fileref2) = @_;
    my @blocklist;

    print "***************\n";
    # Calculate item number range.
    my $range1 = $hunk->context_range(1);
    my $range2 = $hunk->context_range(2);

    # Print out file 1 part for each block in context diff format if there are
    # any blocks that remove items
    print "*** $range1 ****\n";
    my $low = $hunk->{"start1"};
    my $hi  = $hunk->{"end1"};
    if (@blocklist = grep {$_->remove} @{$hunk->{"blocks"}}) {
	my @outlist = @$fileref1[$low..$hi];
	map {s/^/  /} @outlist; # assume it's just context
	foreach my $block (@blocklist) {
	    my $op = $block->op; # - or !
	    foreach my $item ($block->remove) {
		$outlist[$item->{"item_no"} - $low] =~ s/^ /$op/;
	    }
	}
	map {s/$/\n/} @outlist; # add \n's
	print @outlist;
    }

    print "--- $range2 ----\n";
    $low = $hunk->{"start2"};
    $hi  = $hunk->{"end2"};
    if (@blocklist = grep {$_->insert} @{$hunk->{"blocks"}}) {
	my @outlist = @$fileref2[$low..$hi];
	map {s/^/  /} @outlist; # assume it's just context
	foreach my $block (@blocklist) {
	    my $op = $block->op; # + or !
	    foreach my $item ($block->insert) {
		$outlist[$item->{"item_no"} - $low] =~ s/^ /$op/;
	    }
	}
	map {s/$/\n/} @outlist; # add \n's
	print @outlist;
    }
}

sub store_ed_diff {
# ed diff prints out diffs *backwards*. So save them while we're generating
# them, then print them out at the end
    my $hunk = shift;
    unshift @Ed_Hunks, $hunk;
}

sub output_ed_diff {
# This sub is used for ed ('diff -e') OR reverse_ed ('diff -f').
# last arg is type of diff
    my $diff_type = $_[-1];
    my ($hunk, $fileref1, $fileref2) = @_;
    my %op_hash = ('+' => 'a', '-' => 'd', '!' => 'c');

    # Can't be any context for this kind of diff, so each hunk has one block
    my @blocklist = @{$hunk->{"blocks"}};
    warn ("Expecting one block in an ed diff hunk!") if scalar @blocklist != 1;
    my $block = $blocklist[0];
    my $op = $block->op; # +, -, or !

    # Calculate item number range.
    # old diff range is just like a context diff range, except the ranges
    # are on one line with the action between them.
    my $range1 = $hunk->context_range(1);
    $range1 =~ s/,/ / if $diff_type eq "REVERSE_ED";
    my $action = $op_hash{$op} || warn "unknown op $op";
    print ($diff_type eq "ED" ? "$range1$action\n" : "$action$range1\n");

    if ($block->insert) {
	my @outlist = @$fileref2[$hunk->{"start2"}..$hunk->{"end2"}];
	map {s/$/\n/} @outlist; # add \n's
	print @outlist;
	print ".\n"; # end of ed 'c' or 'a' command
    }
}

sub context_range {
# Generate a range of item numbers to print. Only print 1 number if the range
# has only one item in it. Otherwise, it's 'start,end'
# Flag is the number of the file (1 or 2)
    my ($hunk, $flag) = @_;
    my ($start, $end) = ($hunk->{"start$flag"},$hunk->{"end$flag"});
    $start++; $end++;  # index from 1, not zero
    my $range = ($start < $end) ? "$start,$end" : $end;
    return $range;
}

sub unified_range {
# Generate a range of item numbers to print for unified diff
# Print number where block starts, followed by number of lines in the block
# (don't print number of lines if it's 1)
    my ($hunk, $flag) = @_;
    my ($start, $end) = ($hunk->{"start$flag"},$hunk->{"end$flag"});
    $start++; $end++;  # index from 1, not zero
    my $length = $end - $start + 1;
    my $first = $length < 2 ? $end : $start; # strange, but correct...
    my $range = $length== 1 ? $first : "$first,$length";
    return $range;
}
} # end Package Hunk

########
# Package Block. A block is an operation removing, adding, or changing
# a group of items. Basically, this is just a list of changes, where each
# change adds or deletes a single item.
# (Change could be a separate class, but it didn't seem worth it)
{
package Block;
sub new {
# Input is a chunk from &Algorithm::LCS::diff
# Fields in a block:
# length_diff - how much longer file 2 is than file 1 due to this block
# Each change has:
# sign        - '+' for insert, '-' for remove
# item_no     - number of the item in the file (e.g., line number)
# We don't bother storing the text of the item
#
    my ($class,$chunk) = @_;
    my @changes = ();

# This just turns each change into a hash.
    foreach my $item (@$chunk) {
	my ($sign, $item_no, $text) = @$item;
	my $hashref = {"sign" => $sign, "item_no" => $item_no};
	push @changes, $hashref;
    }

    my $block = { "changes" => \@changes };
    bless $block, $class;

    $block->{"length_diff"} = $block->insert - $block->remove;
    return $block;
}


# LOW LEVEL FUNCTIONS
sub op {
# what kind of block is this?
    my $block = shift;
    my $insert = $block->insert;
    my $remove = $block->remove;

    $remove && $insert and return '!';
    $remove and return '-';
    $insert and return '+';
    warn "unknown block type";
    return '^'; # context block
}

# Returns a list of the changes in this block that remove items
# (or the number of removals if called in scalar context)
sub remove { return grep {$_->{"sign"} eq '-'} @{shift->{"changes"}}; }

# Returns a list of the changes in this block that insert items
sub insert { return grep {$_->{"sign"} eq '+'} @{shift->{"changes"}}; }

} # end of package Block
