// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {checkTabLinkVisibility} from './test_util.js';

suite('NetInternalsMainTests', function() {
  test('tab visibility state', function() {
    // Expected visibility state of each tab.
    const tabVisibilityState = {
      events: true,
      proxy: true,
      dns: true,
      sockets: true,
      hsts: true,
      chromeos: isChromeOS,
    };

    checkTabLinkVisibility(tabVisibilityState, true);
  });
});
