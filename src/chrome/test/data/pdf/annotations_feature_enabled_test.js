// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testAnnotationsEnabled() {
    const toolbar = document.body.querySelector('#toolbar');
    chrome.test.assertTrue(toolbar.pdfAnnotationsEnabled);
    chrome.test.assertTrue(
        toolbar.shadowRoot.querySelector('#annotate') != null);
    chrome.test.succeed();
  },
]);
