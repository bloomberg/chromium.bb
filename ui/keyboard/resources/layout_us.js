// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple English virtual keyboard implementation.
 */

/**
 * All keys for the rows of the keyboard.
 * NOTE: every row below should have an aspect of 12.6.
 * @type {Array.<Array.<BaseKey>>}
 */
var KEYS_US = [
  [
    new Key(C('\`'), C('~')),
    new Key(C('1'), C('!')),
    new Key(C('2'), C('@')),
    new Key(C('3'), C('#')),
    new Key(C('4'), C('$')),
    new Key(C('5'), C('%')),
    new Key(C('6'), C('^')),
    new Key(C('7'), C('&')),
    new Key(C('8'), C('*')),
    new Key(C('9'), C('(')),
    new Key(C('0'), C(')')),
    new Key(C('-'), C('_')),
    new Key(C('='), C('+')),
    new SvgKey('backspace', 'Backspace', true /* repeat */)
  ],
  [
    new SvgKey('tab', 'Tab'),
    new Key(C('q'), C('Q')),
    new Key(C('w'), C('W')),
    new Key(C('e'), C('E')),
    new Key(C('r'), C('R')),
    new Key(C('t'), C('T')),
    new Key(C('y'), C('Y')),
    new Key(C('u'), C('U')),
    new Key(C('i'), C('I')),
    new Key(C('o'), C('O')),
    new Key(C('p'), C('P')),
    new Key(C('['), C('{')),
    new Key(C(']'), C('}')),
    new Key(C('\\'), C('|'), 'bar'),
  ],
  [
    new SymbolKey(),
    new Key(C('a'), C('A')),
    new Key(C('s'), C('S')),
    new Key(C('d'), C('D')),
    new Key(C('f'), C('F')),
    new Key(C('g'), C('G')),
    new Key(C('h'), C('H')),
    new Key(C('j'), C('J')),
    new Key(C('k'), C('K')),
    new Key(C('l'), C('L')),
    new Key(C(';'), C(':')),
    new Key(C('\''), C('"')),
    new SvgKey('return', 'Enter')
  ],
  [
    new ShiftKey('left-shift'),
    new Key(C('z'), C('Z')),
    new Key(C('x'), C('X')),
    new Key(C('c'), C('C')),
    new Key(C('v'), C('V')),
    new Key(C('b'), C('B')),
    new Key(C('n'), C('N')),
    new Key(C('m'), C('M')),
    new Key(C(','), C('<')),
    new Key(C('.'), C('>')),
    new Key(C('/'), C('?')),
    new ShiftKey('right-shift')
  ],
  [
    new DotComKey(),
    new SpecialKey('at', '@', '@'),
    new SpecialKey('space', ' ', 'Spacebar'),
    new SpecialKey('comma', ',', ','),
    new SpecialKey('period', '.', '.')
  ]
];

// Add layout to KEYBOARDS, which is defined in common.js
KEYBOARDS['us'] = {
  'definition': KEYS_US,
  'aspect': 3.15,
};
