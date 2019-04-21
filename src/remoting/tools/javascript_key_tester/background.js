/* Copyright (c) 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

function createWindow() {
  var windowAttributes = {
    width: 800,
    height: 600
  };
  chrome.app.window.create('main.html', windowAttributes);
};

chrome.app.runtime.onLaunched.addListener(createWindow);
