// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system.memory api test
// extensions_browsertests --gtest_filter=SystemMemoryApiTest.*

chrome.test.runTests([
  function testGet() {
    for(var i = 0; i < 10; ++i) {
      chrome.system.memory.getInfo(chrome.test.callbackPass(function(result) {
        chrome.test.assertEq(4096, result.capacity);
        chrome.test.assertEq(1024, result.availableCapacity);
      }));
    }
  }
]);

