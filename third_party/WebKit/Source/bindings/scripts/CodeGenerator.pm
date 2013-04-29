#
# WebKit IDL parser
#
# Copyright (C) 2005 Nikolas Zimmermann <wildfox@kde.org>
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
# Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
# Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
# Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

package CodeGenerator;

use strict;

use Cwd;
use File::Basename;
use File::Find;
use File::Spec;

my $idlDocument = "";
my $useOutputDir = "";
my $useOutputHeadersDir = "";
my $useDirectories = "";
my $preprocessor;
my $defines = "";
my $verbose = 0;
my $dependentIdlFiles = "";
my $sourceRoot = "";

my $codeGenerator = 0;

# Cache of IDL file pathnames.
my $idlFiles;
my $cachedInterfaces = {};

# Default constructor
sub new
{
    my $object = shift;
    my $reference = { };

    $useDirectories = shift;
    $useOutputDir = shift;
    $useOutputHeadersDir = shift;
    $preprocessor = shift;
    $verbose = shift;
    $dependentIdlFiles = shift;

    $sourceRoot = getcwd();

    bless($reference, $object);
    return $reference;
}

sub ProcessDocument
{
    my $object = shift;
    $idlDocument = shift;
    $defines = shift;

    my $ifaceName = "CodeGeneratorV8";
    require $ifaceName . ".pm";

    # Dynamically load external code generation perl module
    $codeGenerator = $ifaceName->new($object, $idlDocument);
    unless (defined($codeGenerator)) {
        my $interfaces = $idlDocument->interfaces;
        foreach my $interface (@$interfaces) {
            print "Skipping code generation for IDL interface \"" . $interface->name . "\".\n" if $verbose;
        }
        return;
    }

    my $interfaces = $idlDocument->interfaces;
    foreach my $interface (@$interfaces) {
        print "Generating bindings code for IDL interface \"" . $interface->name . "\"...\n" if $verbose;
        $codeGenerator->GenerateInterface($interface, $defines);
        $codeGenerator->WriteData($interface, $useOutputDir, $useOutputHeadersDir);
    }
}

sub IDLFileForInterface
{
    my $interfaceName = shift;

    unless ($idlFiles) {
        my @directories = map { $_ = "$sourceRoot/$_" if -d "$sourceRoot/$_"; $_ } @$useDirectories;
        push(@directories, ".");

        $idlFiles = { };
        foreach my $idlFile (@$dependentIdlFiles) {
            $idlFiles->{fileparse(basename($idlFile), ".idl")} = $idlFile;
        }

        my $wanted = sub {
            $idlFiles->{$1} = $File::Find::name if /^([A-Z].*)\.idl$/;
            $File::Find::prune = 1 if /^\../;
        };
        find($wanted, @directories);
    }

    return $idlFiles->{$interfaceName};
}

sub HeaderFileForInterface
{
    my $object = shift;
    my $interfaceName = shift;

    my $idlFilename = IDLFileForInterface($interfaceName)
        or die("Could NOT find IDL file for interface \"$interfaceName\" $!\n");

    $idlFilename = "bindings/" . File::Spec->abs2rel($idlFilename, $sourceRoot);
    $idlFilename =~ s/idl$/h/;
    return $idlFilename;
}

sub ParseInterface
{
    my ($object, $interfaceName) = @_;

    return undef if $interfaceName eq 'Object';

    if (exists $cachedInterfaces->{$interfaceName}) {
        return $cachedInterfaces->{$interfaceName};
    }

    # Step #1: Find the IDL file associated with 'interface'
    my $filename = IDLFileForInterface($interfaceName)
        or die("Could NOT find IDL file for interface \"$interfaceName\" $!\n");

    print "  |  |>  Parsing parent IDL \"$filename\" for interface \"$interfaceName\"\n" if $verbose;

    # Step #2: Parse the found IDL file (in quiet mode).
    my $parser = IDLParser->new(1);
    my $document = $parser->Parse($filename, $defines, $preprocessor);

    foreach my $interface (@{$document->interfaces}) {
        if ($interface->name eq $interfaceName) {
            $cachedInterfaces->{$interfaceName} = $interface;
            return $interface;
        }
    }

    die("Could NOT find interface definition for $interfaceName in $filename");
}

1;
