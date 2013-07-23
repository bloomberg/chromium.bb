#! /usr/bin/perl
#
#   This file is part of the WebKit project
#
#   Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Library General Public
#   License as published by the Free Software Foundation; either
#   version 2 of the License, or (at your option) any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Library General Public License for more details.
#
#   You should have received a copy of the GNU Library General Public License
#   along with this library; see the file COPYING.LIB.  If not, write to
#   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#   Boston, MA 02110-1301, USA.
use strict;
use warnings;

use File::Basename;
use File::Spec;
use Getopt::Long;

my $outputDir = ".";

GetOptions(
    'outputDir=s' => \$outputDir,
);

my $grammarFilePath = $ARGV[0];
my $grammarIncludesFilePath = @ARGV > 0 ? $ARGV[1] : "";

my ($filename, $basePath, $suffix) = fileparse($grammarFilePath, (".y", ".y.in"));

my $grammarFileOutPath = File::Spec->join($outputDir, "$filename.y");
if (!$grammarIncludesFilePath) {
    $grammarIncludesFilePath = "${basePath}${filename}.y.includes";
}

open GRAMMAR, ">$grammarFileOutPath" or die;
open INCLUDES, "<$grammarIncludesFilePath" or die;

while (<INCLUDES>) {
    print GRAMMAR;
}

open GRAMMARFILE, "<$grammarFilePath" or die;
while (<GRAMMARFILE>) {
    print GRAMMAR;
}

close GRAMMAR;

$grammarFilePath = $grammarFileOutPath;
