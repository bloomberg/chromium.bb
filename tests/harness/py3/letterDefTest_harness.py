# -*- coding: UTF-8 -*-
"""
Liblouis test to show what is beleaved to be a problem with how letters are defined and their interaction with the always opcode.
"""

import louis

table = 'letterDefTest.ctb'

tests = [
    { # When uplow+always are used the following does not fail.
        'txt':  '⠍⠎',
        'brl':  '⠎⠍',
    }, 
]
