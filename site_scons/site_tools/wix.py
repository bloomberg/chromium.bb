#!/usr/bin/python
# Copyright 2008, Google Inc.
# All rights reserved.
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

"""
WiX tool for SCons

Acknowledgement: Rob Mensching's code sample at
http://www.scons.org/wiki/WiX_Tool provided invaluable insight
into how to write this sort of thing. Rob's code is a little
less lazy than mine--he parses the .wxs file directly, while
I just run the Candle in preprocessor mode and capture filenames
with a regex. I think his method is faster but mine may be
a little more robust.
"""
import os
import subprocess
import SCons.Defaults
import SCons.Scanner
import re

def wix_depends( src_path, candle_path, wixflags ):
    """Generates a list of dependencies for the given WiX file.
    The algorithm is fairly naive--it assumes that all 'Source'
    attributes represent dependencies--but it seems to work ok
    for our relatively simple needs.


    Arguments:
        src_path - full path to WiX (.wxs) source file
        candle_path - full path to candle.exe
        wixflags - flags to pass to WiX preprocessor
    """
    # Preprocess the .wxs file using candle.exe, the WiX
    # xml processor
    candle_pp_pipe = subprocess.Popen( [
                                            candle_path,
                                            wixflags,
                                            "-p",
                                            src_path
                                        ],
                                        stdout=subprocess.PIPE )

    candle_pp_output = candle_pp_pipe.communicate()[0]

    # Process the output through a regex. We're looking for
    # anything with the pattern Source="<filename>", and
    # we want to accept either double or single quotes.
    sources_regex = re.compile(
            r"[S|s]ource=(?P<quote>[\"|'])(?P<filename>.*)(?P=quote)" )

    matches = sources_regex.findall(candle_pp_output)

    # The matches are returned as a list of tuples.
    # This code changes it into a tuple of lists. We
    # keep the 2nd list and throw the first away, since
    # the first list contains only the captured quote
    # fields.
    files = []
    if len(matches) > 0:
        (garbage, files) = zip( *matches )

    return files


def wix_scanner( node, env, path ):
    """Simple scanner for WiX (.wxs) files

    Arguments:
        node - the SCons node to scan
        env  - the current SCons environment
        path - unused
    """
    files = wix_depends(
                    str(node),
                    env.subst( '$WIX_CANDLE_PATH' ),
                    env.subst('$WIXFLAGS') )
    return files


def generate(env):
    """SCons entry function. This name is required by SCons."""
    # set the default path to the WiX toolkit
    env.SetDefault( WIX_PATH = os.path.join ("${SOURCE_ROOT}",
                                            "third_party",
                                            "wix_2_0_4221",
                                            "files" ) )

    # define the paths to the candle and light executables,
    # and set them into the environment (we'll need them later)
    candle_exe = os.path.join( "${WIX_PATH}", "candle.exe" )
    env.SetDefault( WIX_CANDLE_PATH = candle_exe )
    light_exe = os.path.join( "${WIX_PATH}", "light.exe" )
    env.SetDefault( WIX_LIGHT_PATH = light_exe )

    # Create a dependency scanner for .wxs files and add it
    # to the current environment
    scanner = SCons.Scanner.Scanner( function = wix_scanner,
                    skeys=[".wxs"] )
    env.Append( SCANNERS = scanner )

    # WiX works in two stages:
    #    1. Candle.exe preprocesses the WiX XML into the tedious and
    #        verbose XML relational database format that the MSI tools
    #        expect. The output is a .wixobj file.
    #    2. Light.exe processes the .wixobj and builds the final MSI
    #        package.
    #
    # We define both stages separately, but only add the final "Light"
    # stage to the environment. The "Candle" stage doesn't need to go
    # directly into the environment, because it's linked to the "Light"
    # stage via the src_builder argument.
    Candle = SCons.Builder.Builder(
                action = candle_exe + " $SOURCE $WIXFLAGS -out $TARGET",
                src_suffix = "wxs",
                suffix = "wixobj" )
    Light = SCons.Builder.Builder(
                action = light_exe + " $SOURCE $WIXFLAGS -out $TARGET",
                src_suffix = '.wixobj',
                src_builder = Candle )

    # Add the Light builder to the environment, but call it Package
    # since "Light" isn't likely to have meaning for anyone who's not
    # already steeped in WiX lore. Plus, it's a replacement for the
    # built-in SCons Package builder, which also creates MSI files
    # but doesn't have the same flexibility as WiX.
    env.Append( BUILDERS = {'Package':Light} )
