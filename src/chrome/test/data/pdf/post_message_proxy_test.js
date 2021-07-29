// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {PDFViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

const viewer =
    /** @type {!PDFViewerElement} */ (document.body.querySelector('#viewer'));

const tests = [
  async function testNoToken() {
    const whenConnectionDenied =
        eventToPromise('connection-denied-for-testing', viewer);
    window.postMessage({type: 'connect'});
    await whenConnectionDenied;
    chrome.test.succeed();
  },
  async function testBadToken() {
    const whenConnectionDenied =
        eventToPromise('connection-denied-for-testing', viewer);
    window.postMessage({type: 'connect', token: 'foo'});
    await whenConnectionDenied;
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
