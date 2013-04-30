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

my $codeGenerator = 0;

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
    $codeGenerator = $ifaceName->new($object, $idlDocument, $useDirectories, $preprocessor, $defines, $verbose, $dependentIdlFiles);
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

1;
