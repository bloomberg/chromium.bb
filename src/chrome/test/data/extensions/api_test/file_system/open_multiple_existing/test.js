// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openFile() {
    chrome.fileSystem.chooseEntry(
        {acceptsMultiple: true},
        chrome.test.callbackPass(function(entries) {
          chrome.test.assertEq(2, entries.length);
          checkEntry(entries[0], 'open_existing1.txt', false, false);
          checkEntry(entries[1], 'open_existing2.txt', false, false);
        })
    );
  }
]);
