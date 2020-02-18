// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

webkitStorageInfo.requestQuota(PERSISTENT, 1, pass, fail);

function pass() {
  console.log("PASS");
  if (window.chrome && chrome.test && chrome.test.succeed)
    chrome.test.succeed();
}

function fail() {
  console.log("FAIL");
  if (window.chrome && chrome.test && chrome.test.fail)
    chrome.test.fail();
}
