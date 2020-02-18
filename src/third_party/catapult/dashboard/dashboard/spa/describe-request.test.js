/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {DescribeRequest} from './describe-request.js';
import {assert} from 'chai';

suite('DescribeRequest', function() {
  test('mergeDescriptor', async function() {
    const merged = {
      measurements: new Set(['measurementA', 'measurementB']),
      bots: new Set(['botA', 'botB']),
      cases: new Set(['caseA', 'caseB']),
      caseTags: new Map([
        ['tagA', new Set(['caseA', 'caseB'])],
      ]),
    };
    DescribeRequest.mergeDescriptor(merged, {
      bots: ['botB', 'botC'],
      measurements: ['measurementB', 'measurementC'],
      cases: ['caseB', 'caseC'],
      caseTags: {
        tagA: ['caseB', 'caseC'],
        tagB: ['caseC'],
      },
    });
    assert.deepEqual(['measurementA', 'measurementB', 'measurementC'],
        [...merged.measurements]);
    assert.deepEqual(['botA', 'botB', 'botC'], [...merged.bots]);
    assert.deepEqual(['caseA', 'caseB', 'caseC'], [...merged.cases]);
    assert.deepEqual(['caseA', 'caseB', 'caseC'],
        [...merged.caseTags.get('tagA')]);
    assert.deepEqual(['caseC'], [...merged.caseTags.get('tagB')]);
  });
});
