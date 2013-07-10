# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Converts the intermediate representation of IDLs between Perl and JSON, for:
# 1. Modularity between parser and code generator; and
# 2. Piecemeal porting to Python, by letting us connect Perl and Python scripts.

use strict;
use warnings;

use Class::Struct;
use JSON -convert_blessed_universally;  # IR contains objects (blessed references)

sub serializeJSON
{
    my $document = shift;
    my $json = JSON->new->utf8;
    # JSON.pm defaults to dying on objects (blessed references) and returning
    # keys in indeterminate order. We set options to change this:
    # allow_blessed: don't die when encounter a blessed reference
    #                (but default to return null)
    # convert_blessed: convert blessed reference as if unblessed
    #                  (rather than returning null)
    # canonical: sort keys when writing JSON, so JSON always in same order,
    #            so can compare output between runs or between Perl and Python
    $json = $json->allow_blessed->convert_blessed->canonical();
    return $json->encode($document);
}

sub deserializeJSON
{
    my $jsonText = shift;
    my $json = JSON->new->utf8;
    my $jsonHash = $json->decode($jsonText);
    return jsonToPerl($jsonHash);
}

sub jsonToPerl
{
    # JSON.pm serializes Perl objects as hashes (with keys CLASS::KEY),
    # so we need to rebuild objects when deserializing
    my $jsonData = shift;

    if (ref $jsonData eq "ARRAY") {
        return [map(jsonToPerl($_), @$jsonData)];
    }

    if (ref $jsonData eq "HASH") {
        my @keys = keys %$jsonData;
        return {} unless @keys;

        my $class = determineClassFromKeys(@keys);
        return jsonHashToPerlObject($jsonData, $class) if $class;

        # just a hash
        my $hashRef = {};
        foreach my $key (@keys) {
            $hashRef->{$key} = jsonToPerl($jsonData->{$key});
        }
        return $hashRef;
    }

    die "Unexpected reference type: " . ref $jsonData . "\n" if ref $jsonData;

    return $jsonData;
}

sub determineClassFromKeys
{
    my @keys = shift;

    # Detect objects as hashes where all keys are of the form CLASS::KEY.
    my $firstKey = $keys[0];
    my $isObject = $firstKey =~ /::/;

    return unless $isObject;

    my $class = (split('::', $firstKey))[0];
    return $class;
}

sub jsonHashToPerlObject
{
    # JSON.pm serializes hash objects of class CLASS as a hash with keys
    # CLASS::KEY1, CLASS::KEY2, etc.
    # When deserializing, need to rebuild objects by stripping prefix
    # and calling the constructor.
    my $jsonHash = shift;
    my $class = shift;

    my %keysValues = ();
    foreach my $classAndKey (keys %{$jsonHash}) {
        my $key = (split('::', $classAndKey))[1];
        $keysValues{$key} = jsonToPerl($jsonHash->{$classAndKey});
    }
    my $object = $class->new(%keysValues);  # Build object
    return $object;
}

1;
