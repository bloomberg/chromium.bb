#!/usr/bin/perl -w
# diffs two files and writes an HTML output file.
use strict;
use CGI qw(:standard :html3);
use Algorithm::Diff 'traverse_sequences';
use Text::Tabs;

my ( @a, @b );

# Take care of whitespace.
sub preprocess
{
	my $arrayRef = shift;
	chomp(@$arrayRef);
	@$arrayRef = expand(@$arrayRef);
}

# This will be called with both lines are the same
sub match
{
	my ( $ia, $ib ) = @_;
	print pre( $a[$ia] ), "\n";
}

# This will be called when there is a line in A that isn't in B
sub only_a
{
	my ( $ia, $ib ) = @_;
	print pre( { -class => 'onlyA' }, $a[$ia] ), "\n";
}

# This will be called when there is a line in B that isn't in A
sub only_b
{
	my ( $ia, $ib ) = @_;
	print pre( { -class => 'onlyB' }, $b[$ib] ), "\n";
}

# MAIN PROGRAM

# Check for two arguments.
print "usage: $0 file1 file2 > diff.html\n" if @ARGV != 2;

$tabstop = 4;    # For Text::Tabs

# Read each file into an array.
open FH, $ARGV[0];
@a = <FH>;
close FH;

open FH, $ARGV[1];
@b = <FH>;
close FH;

# Expand whitespace
preprocess( \@a );
preprocess( \@b );

# inline style
my $style = <<EOS;
	PRE {
		margin-left: 24pt; 
		font-size: 12pt;
	    font-family: Courier, monospaced;
		white-space: pre
    }
	PRE.onlyA { color: red }
	PRE.onlyB { color: blue }
EOS

# Print out the starting HTML
print

  # header(),
  start_html(
	{
		-title => "$ARGV[0] vs. $ARGV[1]",
		-style => { -code => $style }
	}
  ),
  h1(
	{ -style => 'margin-left: 24pt' },
	span( { -style => 'color: red' }, $ARGV[0] ),
	span(" <i>vs.</i> "),
	span( { -style => 'color: blue' }, $ARGV[1] )
  ),
  "\n";

# And compare the arrays
traverse_sequences(
	\@a,    # first sequence
	\@b,    # second sequence
	{
		MATCH     => \&match,     # callback on identical lines
		DISCARD_A => \&only_a,    # callback on A-only
		DISCARD_B => \&only_b,    # callback on B-only
	}
);

print end_html();
