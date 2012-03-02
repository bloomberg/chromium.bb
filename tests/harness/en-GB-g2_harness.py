# -*- coding: utf-8 -*-

"""
Liblouis test harness for the U.K. English Grade 2 Braille Contraction Table

Please see the liblouis documentationfor more information.
"""

import sys
import louis

table = 'en-GB-g2.ctb'

tests = [
    { # check that "the" is correctly contracted
        'txt':  u'the cat sat on the mat', 
        'brl':  u'! cat sat on ! mat'
    }, 
    { # Checking "to" is contracted correctly and joined to next word.
        'txt': u'to the moon',
        'brl': u'6! moon'
    },
    { # Check that "to" at end of line doesnt get contracted, and that "went" is expanded when cursor is positioned within the word.
        'txt': u'you went to',
        'mode': louis.compbrlAtCursor,
        'cursorPos': 4,
        'brl': u'y went to',
        'BRLCursorPos': 2,
    }
]
