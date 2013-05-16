#!/usr/bin/perl -w
#
# Copyright (C) 2011 Google Inc.  All rights reserved.
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
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.
#

use strict;

use File::Basename;
use Getopt::Long;
use Cwd;

my $defines;
my $preprocessor;
my $idlFilesList;
my $supplementalDependencyFile;
my $windowConstructorsFile;
my $writeFileOnlyIfChanged;

GetOptions('defines=s' => \$defines,
           'preprocessor=s' => \$preprocessor,
           'idlFilesList=s' => \$idlFilesList,
           'supplementalDependencyFile=s' => \$supplementalDependencyFile,
           'windowConstructorsFile=s' => \$windowConstructorsFile,
           'writeFileOnlyIfChanged=s' => \$writeFileOnlyIfChanged);

die('Must specify an output file using --supplementalDependencyFile.') unless defined($supplementalDependencyFile);
die('Must specify an output file using --windowConstructorsFile.') unless defined($windowConstructorsFile);
die('Must specify the file listing all IDLs using --idlFilesList.') unless defined($idlFilesList);

open FH, "< $idlFilesList" or die "Cannot open $idlFilesList\n";
my @idlFiles = <FH>;
chomp(@idlFiles);
close FH;

# Parse all IDL files.
my %interfaceNameToIdlFile;
my %idlFileToInterfaceName;
my %supplementalDependencies;
my %supplementals;
my $constructorAttributesCode = "";
foreach my $idlFile (@idlFiles) {
    my $fullPath = Cwd::realpath($idlFile);
    my $idlFileContents = getFileContents($fullPath);
    my $partialInterfaceName = getPartialInterfaceNameFromIDL($idlFileContents);
    if ($partialInterfaceName) {
        $supplementalDependencies{$fullPath} = $partialInterfaceName;
        next;
    }
    my $interfaceName = fileparse(basename($idlFile), ".idl");
    unless (isCallbackInterfaceFromIDL($idlFileContents)) {
        my $extendedAttributes = getInterfaceExtendedAttributesFromIDL($idlFileContents);
        unless ($extendedAttributes->{"NoInterfaceObject"}) {
            $constructorAttributesCode .= GenerateConstructorAttribute($interfaceName, $extendedAttributes);
        }
    }
    $interfaceNameToIdlFile{$interfaceName} = $fullPath;
    $idlFileToInterfaceName{$fullPath} = $interfaceName;
    $supplementals{$fullPath} = [];
}

my $contents = "partial interface DOMWindow {\n$constructorAttributesCode};\n";
WriteFileIfChanged($windowConstructorsFile, $contents);
$supplementalDependencies{$windowConstructorsFile} = "DOMWindow" if $interfaceNameToIdlFile{"DOMWindow"};

# Resolves partial interfaces dependencies.
foreach my $idlFile (keys %supplementalDependencies) {
    my $baseFile = $supplementalDependencies{$idlFile};
    my $targetIdlFile = $interfaceNameToIdlFile{$baseFile};
    push(@{$supplementals{$targetIdlFile}}, $idlFile);
    delete $supplementals{$idlFile};
}

# Outputs the dependency.
# The format of a supplemental dependency file:
#
# DOMWindow.idl P.idl Q.idl R.idl
# Document.idl S.idl
# Event.idl
# ...
#
# The above indicates that DOMWindow.idl is supplemented by P.idl, Q.idl and R.idl,
# Document.idl is supplemented by S.idl, and Event.idl is supplemented by no IDLs.
# The IDL that supplements another IDL (e.g. P.idl) never appears in the dependency file.

$contents = "";
foreach my $idlFile (sort keys %supplementals) {
    $contents .= "$idlFile @{$supplementals{$idlFile}}\n";
}
WriteFileIfChanged($supplementalDependencyFile, $contents);

sub GenerateConstructorAttribute
{
    my $interfaceName = shift;
    my $extendedAttributes = shift;

    my $code = "    ";
    my @extendedAttributesList;
    foreach my $attributeName (keys %{$extendedAttributes}) {
      next unless ($attributeName eq "Conditional" || $attributeName eq "EnabledAtRuntime" || $attributeName eq "EnabledPerContext");
      my $extendedAttribute = $attributeName;
      $extendedAttribute .= "=" . $extendedAttributes->{$attributeName} unless $extendedAttributes->{$attributeName} eq "VALUE_IS_MISSING";
      push(@extendedAttributesList, $extendedAttribute);
    }
    $code .= "[" . join(', ', @extendedAttributesList) . "] " if @extendedAttributesList;

    my $originalInterfaceName = $interfaceName;
    $interfaceName = $extendedAttributes->{"InterfaceName"} if $extendedAttributes->{"InterfaceName"};
    $code .= "attribute " . $originalInterfaceName . "Constructor $interfaceName;\n";

    # In addition to the regular property, for every [NamedConstructor] extended attribute on an interface,
    # a corresponding property MUST exist on the ECMAScript global object.
    if ($extendedAttributes->{"NamedConstructor"}) {
        my $constructorName = $extendedAttributes->{"NamedConstructor"};
        $constructorName =~ s/\(.*//g; # Extract function name.
        $code .= "    ";
        $code .= "[" . join(', ', @extendedAttributesList) . "] " if @extendedAttributesList;
        $code .= "attribute " . $originalInterfaceName . "ConstructorConstructor $constructorName;\n";
    }
    return $code;
}

sub getFileContents
{
    my $idlFile = shift;

    open FILE, "<", $idlFile;
    my @lines = <FILE>;
    close FILE;

    # Filter out preprocessor lines.
    @lines = grep(!/^\s*#/, @lines);

    return join('', @lines);
}

sub getPartialInterfaceNameFromIDL
{
    my $fileContents = shift;

    if ($fileContents =~ /partial\s+interface\s+(\w+)/gs) {
        return $1;
    }
}

sub isCallbackInterfaceFromIDL
{
    my $fileContents = shift;
    return ($fileContents =~ /callback\s+interface\s+\w+/gs);
}

sub trim
{
    my $string = shift;
    $string =~ s/^\s+|\s+$//g;
    return $string;
}

sub getInterfaceExtendedAttributesFromIDL
{
    my $fileContents = shift;

    my $extendedAttributes = {};

    if ($fileContents =~ /\[(.*)\]\s+(interface|exception)\s+(\w+)/gs) {
        my @parts = split(',', $1);
        foreach my $part (@parts) {
            my @keyValue = split('=', $part);
            my $key = trim($keyValue[0]);
            next unless length($key);
            my $value = "VALUE_IS_MISSING";
            $value = trim($keyValue[1]) if @keyValue > 1;
            $extendedAttributes->{$key} = $value;
        }
    }

    return $extendedAttributes;
}

sub WriteFileIfChanged
{
    my $fileName = shift;
    my $contents = shift;

    if (-f $fileName && $writeFileOnlyIfChanged) {
        open FH, "<", $fileName or die "Couldn't open $fileName: $!\n";
        my @lines = <FH>;
        my $oldContents = join "", @lines;
        close FH;
        return if $contents eq $oldContents;
    }
    open FH, ">", $fileName or die "Couldn't open $fileName: $!\n";
    print FH $contents;
    close FH;
}
