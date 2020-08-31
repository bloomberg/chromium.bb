#!/usr/bin/perl -w
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
# Command-line arguments and context lines feature added
# September 1998 Amir D. Karger (karger@bead.aecom.yu.edu)
#
# In this file, "item" usually means "line of text", and "item number" usually
# means "line number". But theoretically the code could be used more generally
use strict;

use Algorithm::Diff qw(diff);
use File::stat;
use vars qw ($opt_C $opt_c $opt_u $opt_U);
use Getopt::Std;

my $usage = << "ENDUSAGE";
Usage: $0 [{-c | -u}] [{-C | -U} lines] oldfile newfile
    -c will do a context diff with 3 lines of context
    -C will do a context diff with 'lines' lines of context
    -u will do a unified diff with 3 lines of context
    -U will do a unified diff with 'lines' lines of context
ENDUSAGE

getopts('U:C:cu') or bag("$usage");
bag("$usage") unless @ARGV == 2;
my ($file1, $file2) = @ARGV;
if (defined $opt_C || defined $opt_c) {
    $opt_c = ""; # -c on if -C given on command line
    $opt_u = undef;
} elsif (defined $opt_U || defined $opt_u) {
    $opt_u = ""; # -u on if -U given on command line
    $opt_c = undef;
} else {
    $opt_c = ""; # by default, do context diff, not old diff
}

my ($char1, $char2); # string to print before file names
my $Context_Lines; # lines of context to print
if (defined $opt_c) {
    $Context_Lines = defined $opt_C ? $opt_C : 3;
    $char1 = '*' x 3; $char2 = '-' x 3;
} elsif (defined $opt_u) {
    $Context_Lines = defined $opt_U ? $opt_U : 3;
    $char1 = '-' x 3; $char2 = '+' x 3;
}

# After we've read up to a certain point in each file, the number of items
# we've read from each file will differ by $FLD (could be 0)
my $File_Length_Difference = 0;

open (F1, $file1) or bag("Couldn't open $file1: $!");
open (F2, $file2) or bag("Couldn't open $file2: $!");
my (@f1, @f2);
chomp(@f1 = <F1>);
close F1;
chomp(@f2 = <F2>);
close F2;

# diff yields lots of pieces, each of which is basically a Block object
my $diffs = diff(\@f1, \@f2);
exit 0 unless @$diffs;

my $st = stat($file1);
print "$char1 $file1\t", scalar localtime($st->mtime), "\n";
$st = stat($file2);
print "$char2 $file2\t", scalar localtime($st->mtime), "\n";

my ($hunk,$oldhunk);
# Loop over hunks. If a hunk overlaps with the last hunk, join them.
# Otherwise, print out the old one.
foreach my $piece (@$diffs) {
    $hunk = new Hunk ($piece, $Context_Lines);
    next unless $oldhunk;

    if ($hunk->does_overlap($oldhunk)) {
	$hunk->prepend_hunk($oldhunk);
    } else {
	$oldhunk->output_diff(\@f1, \@f2);
    }

} continue {
    $oldhunk = $hunk;
}

# print the last hunk
$oldhunk->output_diff(\@f1, \@f2);
exit 1;
# END MAIN PROGRAM

sub bag {
  my $msg = shift;
  $msg .= "\n";
  warn $msg;
  exit 2;
}

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
    # the current difference in file lengths
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
    if    (defined $main::opt_u) {&output_unified_diff(@_)}
    elsif (defined $main::opt_c) {&output_context_diff(@_)}
    else {die "unknown diff"}
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

sub context_range {
# Generate a range of item numbers to print. Only print 1 number if the range
# has only one item in it. Otherwise, it's 'start,end'
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

