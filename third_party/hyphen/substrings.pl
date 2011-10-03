#!/usr/bin/perl
# convert TeX (Patgen) hyphenation patterns to Libhnj format
# (A utility for finding substring embeddings in patterns)
# usage: substrings.pl inputfile outputfile [encoding]

# Libhnj is dual licensed under LGPL and MPL. Boilerplate for both
# licenses follows.

# LibHnj - a library for high quality hyphenation and justification
# Copyright (C) 1998 Raph Levien,
# 	     (C) 2001 ALTLinux, Moscow (http://www.alt-linux.org),
#           (C) 2001 Peter Novodvorsky (nidd@cs.msu.su)
#           (C) 2006, 2007, 2008, 2010 László Németh (nemeth at OOo)
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA  02111-1307  USA.

# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "MPL"); you may not use this file except in
# compliance with the MPL.  You may obtain a copy of the MPL at
# http://www.mozilla.org/MPL/
#
# Software distributed under the MPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
# for the specific language governing rights and limitations under the
# MPL.

if (!defined $ARGV[1]) {
    print "" .
"substrings.pl - convert TeX (Patgen) hyphenation patterns to Libhnj format\n" . 
"(A utility for finding substring embeddings in patterns)\n" .
"usage: substrings.pl infile outfile [encoding [lefthyphenmin [righthyphenmin]]]\n";
    exit 1;
}
$fn = $ARGV[0];
if (!-e $fn) { $fn = "hyphen.us"; }
open HYPH, $fn;
open OUT, ">$ARGV[1]";
$encoding = $ARGV[2];
$lhmin = $ARGV[3];
$rhmin = $ARGV[4];
if (defined $encoding) { print OUT "$encoding\n"; }
if (defined $lhmin) { print OUT "LEFTHYPHENMIN $lhmin\n"; }
if (defined $rhmin) { print OUT "RIGHTHYPHENMIN $rhmin\n"; }

while (<HYPH>)
{
    $pat =~ s/%.*$//g;
    if (/^\%/) {
	#comment, ignore
    } elsif (/^(.+)\/([^,]+),([0-9]+),([0-9]+)$/) {
        $origpat = $1;
	$pat = $1;
        $repl = $2;
        $beg = $3;
        $len = $4;
	$pat =~ s/\d//g;
        if ($origpat eq $pat) {
            print "error - missing hyphenation point: $_";
            exit 1;
        }
	push @patlist, $pat;
	$pattab{$pat} = $origpat;
        $repltab{$pat} = $repl;
        $replbeg{$pat} = $beg - 1;
        $repllen{$pat} = $len;
    } elsif (/^(.+)\/(.+)$/) {
        $origpat = $1;
	$pat = $1;
        $repl = $2;
	$pat =~ s/\d//g;
        if ($origpat eq $pat) {
            print "error - missing hyphenation point: $_";
            exit 1;
        }
	push @patlist, $pat;
	$pattab{$pat} = $origpat;
        $repltab{$pat} = $repl;
        $replbeg{$pat} = 0;
        $repllen{$pat} = enclen($pat);
    } elsif (/^(.+)$/) {
	$origpat = $1;
	$pat = $1;
	$pat =~ s/\d//g;
	push @patlist, $pat;
	$pattab{$pat} = $origpat;
    }
}

foreach $pat (@patlist) {
    $patsize = length $pat;
    for $i (0..$patsize - 1) {
	for $j (1..$patsize - $i) {
	    $subpat = substr ($pat, $i, $j);
	    if (defined $pattab{$subpat}) {
		print "$pattab{$subpat} is embedded in $pattab{$pat}\n";
		$newpat = substr $pat, 0, $i + $j;
		if (!defined $newpattab{$newpat}) {
		    $newpattab{$newpat} =
			substr ($pat, 0, $i).$pattab{$subpat};
		    $ss = substr $pat, 0, $i;
		    print "$ss+$pattab{$subpat}\n";
		    push @newpatlist, $newpat;
		    if (defined $repltab{$subpat}) {
                        $begcorr = (($pat =~ /^[.]/) && !($subpat =~ /^[.]/)) ? 1 : 0;
                        $newrepltab{$newpat} = $repltab{$subpat};
                        $newreplbeg{$newpat} = $replbeg{$subpat} + enclen($ss) - $begcorr;
                        $newrepllen{$newpat} = $repllen{$subpat};
                    }
		} else {
		    $tmp =  $newpattab{$newpat};
		    $newpattab{$newpat} =
			combine ($newpattab{$newpat}, $pattab{$subpat});
		    print "$tmp + $pattab{$subpat} -> $newpattab{$newpat}\n";
		}
	    }
	}
    }
}

foreach $pat (@newpatlist) {
    if (defined $newrepltab{$pat}) {
        print OUT $newpattab{$pat}."/".$newrepltab{$pat}.",".($newreplbeg{$pat}+1).",".$newrepllen{$pat}."\n";
    } else {
        print OUT $newpattab{$pat}."\n";
    }
}

#convert 'n1im' to 0n1i0m0 expresed as a list
sub expand {
    my ($pat) = @_;
    my $last = '.';
    my @exp = ();

    foreach $c (split (//, $pat)) {
	if ($last =~ /[\D]/ && $c =~ /[\D]/) {
	    push @exp, 0;
	}
	push @exp, $c;
	$last = $c;
    }
    if ($last =~ /[\D]/) {
	push @exp, 0;
    }
    return @exp;
}

# Combine two patterns, i.e. .ad4der + a2d becomes .a2d4der
# The second pattern needs to be a substring of the first (modulo digits)
sub combine {
    my @exp = expand shift;
    my @subexp = expand shift;
    my $pat1, $pat2;
    my $i;

    $pat1 = join ('', map { $_ =~ /\d/ ? () : $_ } @exp);
    $pat2 = join ('', map { $_ =~ /\d/ ? () : $_ } @subexp);

    $begcorr = ($pat1 =~ /^[.]/) ? 1 : 0;

    for $i (0..length ($pat1) - length ($pat2)) {
	if (substr ($pat1, $i, length $pat2) eq $subpat) {
	    for ($j = 0; $j < @subexp; $j += 2) {
		if ($subexp[$j] > $exp[2 * $i + $j]) {
		    $exp[2 * $i + $j] = $subexp[$j];
                    if (defined $newrepltab{$pat2} && !defined $newrepltab{$pat1}) {
                        $ss = substr ($pat1, 0, $i);
                        $newrepltab{$pat1} = $newrepltab{$pat2};
                        $newreplbeg{$pat1} = $newreplbeg{$pat2} + enclen($ss) - $begcorr;
                        $newrepllen{$pat1} = $newrepllen{$pat2};
                    }
		}
	    }
	    print ("$pat1 includes $pat2 at pos $i\n");
	}
    }
    return join ('', map { $_ eq '0' ? () : $_ } @exp);
}

# 8 bit or UTF-8 character length (calculating right start position for discretionary hyphenation)
sub enclen {
    my $nonchar = 0;
    my $len = length($_[0]);
    if ($encoding eq "UTF-8") {
        # length of an UTF-8 string equals to the count of the characters not started with '10' bits
        for ($i = 0; $i < $len; $i++) {
            if ((ord(substr($_[0], $i, 1)) >> 6) == 2) { $nonchar++; }
        }
    }
    return $len - $nonchar;   
}
