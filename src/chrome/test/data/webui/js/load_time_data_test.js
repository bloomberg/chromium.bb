// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import 'chrome://resources/js/load_time_data.m.js';

suite('LoadTimeDataModuleTest', function() {
  const loadTimeData = window.loadTimeData;

  test('getStringPieces', function() {
    const assertSubstitutedPieces = function(expected, var_args) {
      var var_args = Array.prototype.slice.call(arguments, 1);
      var pieces =
          loadTimeData.getSubstitutedStringPieces.apply(loadTimeData, var_args);
      assertDeepEquals(expected, pieces);

      // Ensure output matches getStringF.
      assertEquals(
          loadTimeData.substituteString.apply(loadTimeData, var_args),
          pieces.map(p => p.value).join(''));
    };

    assertSubstitutedPieces([{value: 'paper', arg: null}], 'paper');
    assertSubstitutedPieces([{value: 'paper', arg: '$1'}], '$1', 'paper');

    assertSubstitutedPieces(
        [
          {value: 'i think ', arg: null},
          {value: 'paper mario', arg: '$1'},
          {value: ' is a good game', arg: null},
        ],
        'i think $1 is a good game', 'paper mario');

    assertSubstitutedPieces(
        [
          {value: 'paper mario', arg: '$1'},
          {value: ' costs $', arg: null},
          {value: '60', arg: '$2'},
        ],
        '$1 costs $$$2', 'paper mario', '60');

    assertSubstitutedPieces(
        [
          {value: 'paper mario', arg: '$1'},
          {value: ' costs $60', arg: null},
        ],
        '$1 costs $$60', 'paper mario');

    assertSubstitutedPieces(
        [
          {value: 'paper mario', arg: '$1'},
          {value: ' costs\n$60 ', arg: null},
          {value: 'today', arg: '$2'},
        ],
        '$1 costs\n$$60 $2', 'paper mario', 'today');

    assertSubstitutedPieces(
        [
          {value: '$$', arg: null},
          {value: '1', arg: '$1'},
          {value: '2', arg: '$2'},
          {value: '1', arg: '$1'},
          {value: '$$2', arg: null},
          {value: '2', arg: '$2'},
          {value: '$', arg: null},
          {value: '1', arg: '$1'},
          {value: '$', arg: null},
        ],
        '$$$$$1$2$1$$$$2$2$$$1$$', '1', '2');
  });

  test('unescapedDollarSign', function() {
    const error = 'Unexpected condition on ' + window.location.href +
        ': Unescaped $ found in localized string.';
    /** @param {string} label */
    const assertSubstitutionThrows = function(label) {
      assertThrows(() => {
        loadTimeData.getSubstitutedStringPieces(label);
      }, error);

      assertThrows(() => {
        loadTimeData.substituteString(label);
      }, error);
    };

    assertSubstitutionThrows('$');
    assertSubstitutionThrows('$1$$$a2');
    assertSubstitutionThrows('$$$');
    assertSubstitutionThrows('a$');
    assertSubstitutionThrows('a$\n');
  });
});
