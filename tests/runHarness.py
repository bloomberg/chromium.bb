#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Liblouis test harness
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
# Free Software Foundation, Inc., Franklin Street, Fifth Floor,
# Boston MA  02110-1301 USA.
#
# Copyright (c) 2012, liblouis team, Mesar Hameed.

"""Liblouis test harness:
Please see the liblouis documentation for information of how to add a new harness or more tests for your braille table.

@author: Mesar Hameed <mhameed@src.gnome.org>
@author: Michael Whapples <mwhapples@aim.com>
"""

import sys
import os

from louis import translate
from glob import iglob
from os.path import basename

def showCurPos(length, pos1, marker1="^", pos2=None, marker2="*"):
    """A helper function to make a string to show the position of the given cursor."""
    display = [" "] *length
    display[pos1] = marker1
    if pos2: display[pos2] = marker2
    return "".join(display)

def reportFailure(text, actualBRL, expectedBRL, cursorPos, actualBRLCursorPos, expectedBRLCursorPos):
    """Function to layout and print a failure report.

    Works out where the missmatch is occuring and presents the necessary information, with markers.
    """

    template = "%-25s '%s'"

    report = [template % ("text:", text),
              template %("CursorAt: %d" %cursorPos, showCurPos(len(text), cursorPos) )]
    if actualBRL != expectedBRL and actualBRLCursorPos != expectedBRLCursorPos:
        report.insert(0,"--- Braille and cursor Difference Failure: ---")
        report.extend([
            template % ("expected brl:", expectedBRL),
            template %("expectedCursorAt: %d" %expectedBRLCursorPos, showCurPos(len(expectedBRL), expectedBRLCursorPos) ),

            template % ("actual brl:", actualBRL),
            template %("actualCursorAt: %d" %actualBRLCursorPos, showCurPos(len(actualBRL), actualBRLCursorPos) ),
        ])
    elif actualBRL != expectedBRL:
        report.insert(0,"--- Braille Difference Failure: ---")
        report.extend([
            template % ("expected brl:", expectedBRL),
            template % ("actual brl:", actualBRL),
            template %("brlCursorAt: %d" %actualBRLCursorPos, showCurPos(len(actualBRL), actualBRLCursorPos) ),
        ])
    else: 
        report.insert(0, "--- Braille Cursor Difference Failure: ---")
        report.extend([
            template % ("received brl:", actualBRL),
            template % ("BRLCursorAt %d expected %d:" % (actualBRLCursorPos, expectedBRLCursorPos),
                        showCurPos(len(actualBRL), actualBRLCursorPos, pos2=expectedBRLCursorPos))
        ])
    report.append("--- end ---")
    print("\n".join(report).encode("utf-8"))

total_failed = 0
harness_dir = "harness"
if 'HARNESS_DIR' in os.environ:
    # we assume that if HARNESS_DIR is set that we are invoked from
    # the Makefile, i.e. all the paths to the Python test files and
    # the test tables are set correctly.
    harness_dir = os.environ['HARNESS_DIR']
else:
    # we are not invoked via the Makefile, i.e. we have to set up the
    # paths (PYTHONPATH and LOUIS_TABLEPATH) manually.
    harness_dir = "harness"
    # make sure the harness modules are found in the harness
    # directory, i.e. insert the harness directory into the module
    # search path
    sys.path.insert(1, harness_dir)
    # make sure local test braille tables are found
    os.environ['LOUIS_TABLEPATH'] = 'tables'

# Process all *_harness.py files in the harness directory.
harness_modules = None
if sys.version_info[0] == 2:
    harness_modules = iglob(os.path.join(harness_dir, '*_harness.py'))
else:
    harness_modules = iglob(os.path.join(harness_dir, 'py3', '*_harness.py'))

for harness in harness_modules:
    try:
        harnessModule = __import__(basename(harness)[:-3])
    except Exception as e:
        # Doesn't look like the harness is a valid python file.
        print("Warning: could not import %s" % harness)
        print(e)
        total_failed += 1
        continue
    print("Processing %s" %harness)
    failed = 0
    tableList = [harnessModule.table]
    for test in harnessModule.tests:
        text = test['txt']
        mode = test.get('mode', 0)
        cursorPos = test.get('cursorPos', 0)
        expectedBRLCursorPos = test.get('BRLCursorPos', 0)
        expectedBRL = test['brl']

        actualBRL, BRL2rawPos, raw2BRLPos, actualBRLCursorPos = translate(tableList, text, mode=mode, cursorPos=cursorPos, typeform=None)
        if actualBRL != expectedBRL or actualBRLCursorPos != expectedBRLCursorPos:
            failed += 1 
            reportFailure(text, actualBRL, expectedBRL, cursorPos, actualBRLCursorPos, expectedBRLCursorPos)
    total_failed += failed
    print("%d of %d tests failed." %(failed, len(harnessModule.tests)))

sys.exit(0 if total_failed == 0 else 1)
