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
@author: Hammer Attila <hammera@pickup.hu>
"""

import json
import os
import sys
from louis import translate, backTranslateString
from louis import noContractions, compbrlAtCursor, dotsIO, comp8Dots, pass1Only, compbrlLeftCursor, otherTrans, ucBrl
from glob import iglob

PY2 = sys.version_info[0] == 2

def u(a):
    if PY2:
        return a.encode("utf-8")
    return a

modes = {
    'noContractions': noContractions,
    'compbrlAtCursor': compbrlAtCursor,
    'dotsIO': dotsIO,
    'comp8Dots': comp8Dots,
    'pass1Only': pass1Only,
    'compbrlLeftCursor': compbrlLeftCursor,
    'otherTrans': otherTrans,
    'ucBrl': ucBrl
}

def showCurPos(length, pos1, marker1="^", pos2=None, marker2="*"):
    """A helper function to make a string to show the position of the given cursor."""
    display = [" "] *length
    display[pos1] = marker1
    if pos2:
        display[pos2] = marker2
    return "".join(display)

class BrailleTest():
    def __init__(self, harnessName, table, input, output, mode=0, cursorPos=None, brlCursorPos=None, testmode='translate', comment=None):
        self.harnessName = harnessName
        self.table = table
        self.input = input
        self.expectedBrl = output
        self.mode = mode if not mode else modes[mode]
        self.cursorPos = cursorPos
        self.expectedBrlCursorPos = brlCursorPos
        self.comment = comment
        self.testmode = testmode

    def __str__(self):
        return "%s: %s" % (self.harnessName, self.input)

    def check_translate(self):
        if self.testmode != 'translate': return
        if self.cursorPos is not None:
            tBrl, temp1, temp2, tBrlCurPos = translate(self.table, self.input, mode=self.mode, cursorPos=self.cursorPos)
        else:
            tBrl, temp1, temp2, tBrlCurPos = translate(self.table, self.input, mode=self.mode)
        template = "%-25s '%s'"
        tBrlCurPosStr = showCurPos(len(tBrl), tBrlCurPos)
        report = [
            self.__str__(),
            "--- Braille Difference Failure: ---",
            template % ("expected brl:", self.expectedBrl),
            template % ("actual brl:", tBrl),
            "--- end ---",
        ]
        assert tBrl == self.expectedBrl, u("\n".join(report))

    def check_backtranslate(self):
        backtranslate_output = backTranslateString(self.table, self.expectedBrl, None, mode=self.mode)
        template = "%-25s '%s'"
        report = [
            self.__str__(),
            "--- Backtranslate failure: ---",
            template % ("expected text:", self.input),
            template % ("actual backtranslated text:", backtranslate_output),
            "--- end ---",
        ]
        assert backtranslate_output == self.input, u("\n".join(report))

    def check_cursor(self):
        tBrl, temp1, temp2, tBrlCurPos = translate(self.table, self.input, mode=self.mode, cursorPos=self.cursorPos)
        template = "%-25s '%s'"
        etBrlCurPosStr = showCurPos(len(tBrl), tBrlCurPos, pos2=self.expectedBrlCursorPos)
        report = [
            self.__str__(),
            "--- Braille Cursor Difference Failure: ---",
            template % ("received brl:", tBrl),
            template % ("BRLCursorAt %d expected %d:" %(tBrlCurPos, self.expectedBrlCursorPos), 
                        etBrlCurPosStr),
            "--- end ---"
        ]
        assert tBrlCurPos == self.expectedBrlCursorPos, u("\n".join(report))

def test_allCases():
    harness_dir = "harness"
    if 'HARNESS_DIR' in os.environ:
        # we assume that if HARNESS_DIR is set that we are invoked from
        # the Makefile, i.e. all the paths to the Python test files and
        # the test tables are set correctly.
        harness_dir = os.environ['HARNESS_DIR']
    else:
        # we are not invoked via the Makefile, i.e. we have to set up the
        # paths (LOUIS_TABLEPATH) manually.
        harness_dir = "harness"
        # make sure local test braille tables are found
        os.environ['LOUIS_TABLEPATH'] = 'tables'

    # Process all *_harness.txt files in the harness directory.
    for harness in iglob(os.path.join(harness_dir, '*_harness.txt')):
        f = open(harness, 'r')
        harnessModule = json.load(f, encoding="UTF-8")
        f.close()
        print("Processing %s" %harness)
        tableList = [u(harnessModule['table'])]
        origflags = {'testmode':'translate'}
        for section in harnessModule['sections']:
            flags = section.get('flags', origflags)
            for testData in section['tests']:
                test = flags.copy()
                test.update(testData)
                bt = BrailleTest(harness, tableList, **test)
                if test['testmode'] == 'translate':
                    yield bt.check_translate
                    if test.has_key('cursorPos'):
                        yield bt.check_cursor
                if test['testmode'] == 'backtranslate':
                    continue
                    #yield bt.check_backtranslate
