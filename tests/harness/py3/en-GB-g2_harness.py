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
        'txt':  'the cat sat on the mat', 
        'brl':  '! cat sat on ! mat'
    }, 
    { # Checking "to" is contracted correctly and joined to next word.
        'txt': 'to the moon',
        'brl': '6! moon'
    },
    { # Check that "to" at end of line doesnt get contracted, and that "went" is expanded when cursor is positioned within the word.
        'txt': 'you went to',
        'mode': louis.compbrlAtCursor,
        'cursorPos': 4,
        'brl': 'y went to',
        'BRLCursorPos': 2,
    }
]
