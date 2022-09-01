// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('ChromeTimeTicksTest', () => {
  test('NowIsBigInt', () => {
    const now = chrome.timeTicks.nowInMicroseconds();
    assertEquals(typeof now, 'bigint');
  });
});