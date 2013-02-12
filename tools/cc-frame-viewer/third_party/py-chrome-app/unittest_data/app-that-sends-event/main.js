/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('index.html',
                           {width: 500, height: 309});
});
