#!/usr/bin/perl -w
#
# Copyright (C) 2005 Apple Computer, Inc.
# Copyright (C) 2006 Anders Carlsson <andersca@mac.com>
#
# This file is part of WebKit
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

use File::Path;
use File::Basename;
use Getopt::Long;
use Text::ParseWords;
use Cwd;

use idl_parser;
use code_generator_v8;
use idl_serializer;

my @idlDirectories;
my $outputDirectory;
my $preprocessor;
my $verbose;
my $interfaceDependenciesFile;
my $additionalIdlFiles;
my $idlAttributesFile;
my $writeFileOnlyIfChanged;

GetOptions('include=s@' => \@idlDirectories,
           'outputDir=s' => \$outputDirectory,
           'preprocessor=s' => \$preprocessor,
           'verbose' => \$verbose,
           'interfaceDependenciesFile=s' => \$interfaceDependenciesFile,
           'additionalIdlFiles=s' => \$additionalIdlFiles,
           'idlAttributesFile=s' => \$idlAttributesFile,
           'write-file-only-if-changed=s' => \$writeFileOnlyIfChanged);

my $targetIdlFile = $ARGV[0];

die('Must specify input file.') unless defined($targetIdlFile);
die('Must specify output directory.') unless defined($outputDirectory);

$targetIdlFile = Cwd::realpath($targetIdlFile);
if ($verbose) {
    print "$targetIdlFile\n";
}
my $targetInterfaceName = fileparse(basename($targetIdlFile), ".idl");

my $idlFound = 0;
my @dependencyIdlFiles;
if ($interfaceDependenciesFile) {
    # The format of the interface dependencies file:
    #
    # Window.idl P.idl Q.idl R.idl
    # Document.idl S.idl
    # Event.idl
    # ...
    #
    # The above indicates that Window.idl depends on P.idl, Q.idl, and R.idl,
    # Document.idl depends on S.idl, and Event.idl depends on no IDLs.
    # A dependency IDL file (one that is depended on by another IDL, e.g. P.idl
    # in the above) does not have its own entry in the dependency file.
    open FH, "< $interfaceDependenciesFile" or die "Cannot open $interfaceDependenciesFile\n";
    while (my $line = <FH>) {
        my ($idlFile, @followingIdlFiles) = split(/\s+/, $line);
        if ($idlFile and basename($idlFile) eq basename($targetIdlFile)) {
            $idlFound = 1;
            # We sort the dependency IDL files so that the corresponding code is generated
            # in a consistent order. This is important for the bindings tests.
            @dependencyIdlFiles = sort @followingIdlFiles;
        }
    }
    close FH;

    # $additionalIdlFiles is list of IDL files which should not be included in
    # DerivedSources*.cpp (i.e. they are not described in the interface
    # dependencies file) but should generate .h and .cpp files.
    if (!$idlFound and $additionalIdlFiles) {
        my @idlFiles = shellwords($additionalIdlFiles);
        $idlFound = grep { $_ and basename($_) eq basename($targetIdlFile) } @idlFiles;
    }

    if (!$idlFound) {
        # We generate empty .h and .cpp files just to tell build scripts that .h and .cpp files are created.
        generateEmptyHeaderAndCpp($targetInterfaceName, $outputDirectory);
        exit 0;
    }
}

# Parse the target IDL file.
my $targetParser = idl_parser->new(!$verbose);
my $targetDocument = $targetParser->Parse($targetIdlFile, $preprocessor);

if ($idlAttributesFile) {
    my $idlAttributes = loadIDLAttributes($idlAttributesFile);
    checkIDLAttributes($idlAttributes, $targetDocument, basename($targetIdlFile));
}

foreach my $idlFile (@dependencyIdlFiles) {
    next if $idlFile eq $targetIdlFile;

    my $interfaceName = fileparse(basename($idlFile), ".idl");
    my $parser = idl_parser->new(!$verbose);
    my $document = $parser->Parse($idlFile, $preprocessor);

    foreach my $interface (@{$document->interfaces}) {
        if (!$interface->isPartial || $interface->name eq $targetInterfaceName) {
            my $targetDataNode;
            foreach my $interface (@{$targetDocument->interfaces}) {
                if ($interface->name eq $targetInterfaceName) {
                    $targetDataNode = $interface;
                    last;
                }
            }
            die "Not found an interface ${targetInterfaceName} in ${targetInterfaceName}.idl." unless defined $targetDataNode;

            # Support for attributes of partial interfaces.
            foreach my $attribute (@{$interface->attributes}) {
                # Record that this attribute is implemented by $interfaceName.
                $attribute->extendedAttributes->{"ImplementedBy"} = $interfaceName unless $interface->extendedAttributes->{"LegacyImplementedInBaseClass"};

                # Add interface-wide extended attributes to each attribute.
                applyInterfaceExtendedAttributes($interface, $attribute->extendedAttributes);

                push(@{$targetDataNode->attributes}, $attribute);
            }

            # Support for methods of partial interfaces.
            foreach my $function (@{$interface->functions}) {
                # Record that this method is implemented by $interfaceName.
                $function->extendedAttributes->{"ImplementedBy"} = $interfaceName unless $interface->extendedAttributes->{"LegacyImplementedInBaseClass"};

                # Add interface-wide extended attributes to each method.
                applyInterfaceExtendedAttributes($interface, $function->extendedAttributes);

                push(@{$targetDataNode->functions}, $function);
            }

            # Support for constants of partial interfaces.
            foreach my $constant (@{$interface->constants}) {
                # Record that this constant is implemented by $interfaceName.
                $constant->extendedAttributes->{"ImplementedBy"} = $interfaceName unless $interface->extendedAttributes->{"LegacyImplementedInBaseClass"};

                # Add interface-wide extended attributes to each constant.
                applyInterfaceExtendedAttributes($interface, $constant->extendedAttributes);

                push(@{$targetDataNode->constants}, $constant);
            }
        } else {
            die "$idlFile is not a dependency of $targetIdlFile. There maybe a bug in the dependency computer (compute_dependencies.py).\n";
        }
    }
}

# Serialize to and from JSON to ensure Perl and Python parsers are equivalent,
# as part of porting compiler to Python. See http://crbug.com/242795
$targetDocument = deserializeJSON(serializeJSON($targetDocument));

# Generate desired output for the target IDL file.
my @interfaceIdlFiles = ($targetDocument->fileName(), @dependencyIdlFiles);
my $codeGenerator = code_generator_v8->new($targetDocument, \@idlDirectories, $preprocessor, $verbose, \@interfaceIdlFiles, $writeFileOnlyIfChanged);
my $interfaces = $targetDocument->interfaces;
foreach my $interface (@$interfaces) {
    print "Generating bindings code for IDL interface \"" . $interface->name . "\"...\n" if $verbose;
    $codeGenerator->GenerateInterface($interface);
    $codeGenerator->WriteData($interface, $outputDirectory);
}

sub generateEmptyHeaderAndCpp
{
    my ($targetInterfaceName, $outputDirectory) = @_;

    my $headerName = "V8${targetInterfaceName}.h";
    my $cppName = "V8${targetInterfaceName}.cpp";
    my $contents = "/*
    This file is generated just to tell build scripts that $headerName and
    $cppName are created for ${targetInterfaceName}.idl, and thus
    prevent the build scripts from trying to generate $headerName and
    $cppName at every build. This file must not be tried to compile.
*/
";
    open FH, "> ${outputDirectory}/${headerName}" or die "Cannot open $headerName\n";
    print FH $contents;
    close FH;

    open FH, "> ${outputDirectory}/${cppName}" or die "Cannot open $cppName\n";
    print FH $contents;
    close FH;
}

sub loadIDLAttributes
{
    my $idlAttributesFile = shift;

    my %idlAttributes;
    open FH, "<", $idlAttributesFile or die "Couldn't open $idlAttributesFile: $!";
    while (my $line = <FH>) {
        chomp $line;
        next if $line =~ /^\s*#/;
        next if $line =~ /^\s*$/;

        if ($line =~ /^\s*([^=\s]*)\s*=?\s*(.*)/) {
            my $name = $1;
            $idlAttributes{$name} = {};
            if ($2) {
                foreach my $rightValue (split /\|/, $2) {
                    $rightValue =~ s/^\s*|\s*$//g;
                    $rightValue = "VALUE_IS_MISSING" unless $rightValue;
                    $idlAttributes{$name}{$rightValue} = 1;
                }
            } else {
                $idlAttributes{$name}{"VALUE_IS_MISSING"} = 1;
            }
        } else {
            die "The format of " . basename($idlAttributesFile) . " is wrong: line $.\n";
        }
    }
    close FH;

    return \%idlAttributes;
}

sub checkIDLAttributes
{
    my $idlAttributes = shift;
    my $document = shift;
    my $idlFile = shift;

    foreach my $interface (@{$document->interfaces}) {
        checkIfIDLAttributesExists($idlAttributes, $interface->extendedAttributes, $idlFile);

        foreach my $attribute (@{$interface->attributes}) {
            checkIfIDLAttributesExists($idlAttributes, $attribute->extendedAttributes, $idlFile);
        }

        foreach my $function (@{$interface->functions}) {
            checkIfIDLAttributesExists($idlAttributes, $function->extendedAttributes, $idlFile);
            foreach my $parameter (@{$function->parameters}) {
                checkIfIDLAttributesExists($idlAttributes, $parameter->extendedAttributes, $idlFile);
            }
        }
    }
}

sub applyInterfaceExtendedAttributes
{
    my $interface = shift;
    my $extendedAttributes = shift;

    foreach my $extendedAttributeName (keys %{$interface->extendedAttributes}) {
        next if $extendedAttributeName eq "ImplementedAs";
        $extendedAttributes->{$extendedAttributeName} = $interface->extendedAttributes->{$extendedAttributeName};
    }
}

sub checkIfIDLAttributesExists
{
    my $idlAttributes = shift;
    my $extendedAttributes = shift;
    my $idlFile = shift;

    my $error;
    OUTER: for my $name (keys %$extendedAttributes) {
        if (!exists $idlAttributes->{$name}) {
            $error = "Invalid IDL attribute [$name] found in $idlFile.";
            last;
        }
        # Check no argument first, since "*" means "some argument (not missing)".
        if ($extendedAttributes->{$name} eq "VALUE_IS_MISSING" and not exists $idlAttributes->{$name}{"VALUE_IS_MISSING"}) {
            $error = "Missing required argument for IDL attribute [$name] in file $idlFile.";
            last;
        }
        if (exists $idlAttributes->{$name}{"*"}) {
            next;
        }
        for my $rightValue (split /\s*[|&]\s*/, $extendedAttributes->{$name}) {
            if (!(exists $idlAttributes->{$name}{$rightValue})) {
                $error = "Invalid IDL attribute value [$name=" . $extendedAttributes->{$name} . "] found in $idlFile.";
                last OUTER;
            }
        }
    }
    if ($error) {
        die "IDL ATTRIBUTE CHECKER ERROR: $error
If you want to add a new IDL attribute, you need to add it to bindings/scripts/IDLAttributes.txt and add explanations to the Blink IDL document (http://chromium.org/blink/webidl).
";
    }
}
