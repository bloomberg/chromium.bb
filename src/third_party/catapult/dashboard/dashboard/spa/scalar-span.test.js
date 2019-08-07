/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {assert} from 'chai';
import ScalarSpan from './scalar-span.js';
import {afterRender} from './utils.js';

suite('scalar-span', function() {
  test('format', async function() {
    const scalarSpan = document.createElement('scalar-span');
    document.body.appendChild(scalarSpan);
    scalarSpan.unit = tr.b.Unit.byName.timeDurationInMsDelta_smallerIsBetter;
    scalarSpan.value = 1.111111;
    await afterRender();
    assert.strictEqual('+1.111 ms', scalarSpan.$.span.textContent.trim());
    assert.strictEqual('regression', scalarSpan.$.span.getAttribute('change'));

    scalarSpan.unit = tr.b.Unit.byName.timeDurationInMsDelta_biggerIsBetter;
    await afterRender();
    assert.strictEqual('improvement', scalarSpan.$.span.getAttribute('change'));
    document.body.removeChild(scalarSpan);
  });

  test('getChange', async function() {
    assert.strictEqual('', ScalarSpan.getChange(undefined, 1));
    assert.strictEqual('', ScalarSpan.getChange(tr.b.Unit.byName.count, 1));
    assert.strictEqual('', ScalarSpan.getChange(
        tr.b.Unit.byName.countDelta, 1));
    assert.strictEqual('regression', ScalarSpan.getChange(
        tr.b.Unit.byName.countDelta_biggerIsBetter, -1));
    assert.strictEqual('regression', ScalarSpan.getChange(
        tr.b.Unit.byName.countDelta_smallerIsBetter, 1));
    assert.strictEqual('improvement', ScalarSpan.getChange(
        tr.b.Unit.byName.countDelta_biggerIsBetter, 1));
    assert.strictEqual('improvement', ScalarSpan.getChange(
        tr.b.Unit.byName.countDelta_smallerIsBetter, -1));
    assert.strictEqual('', ScalarSpan.getChange(
        tr.b.Unit.byName.countDelta_biggerIsBetter, 0));
    assert.strictEqual('', ScalarSpan.getChange(
        tr.b.Unit.byName.countDelta_smallerIsBetter, 0));
  });
});
