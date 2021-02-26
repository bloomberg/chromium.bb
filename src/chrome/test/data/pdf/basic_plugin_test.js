// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFScriptingAPI} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';
import {PDFViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer.js';

/**
 * These tests require that the PDF plugin be available to run correctly.
 */
const tests = [
  /**
   * Test that the page is sized to the size of the document.
   */
  function testPageSize() {
    const viewer = /** @type {!PDFViewerElement} */ (
        document.body.querySelector('#viewer'));
    // Verify that the initial zoom is less than or equal to 100%.
    chrome.test.assertTrue(viewer.viewport.getZoom() <= 1);

    // TODO (https://crbug.com/1120279): Currently, calling setZoom() on the
    // viewport with the new UI enabled triggers a crash in Blink. Fix this
    // issue and remove the lines below.
    if (document.documentElement.hasAttribute('pdf-viewer-update-enabled')) {
      chrome.test.succeed();
      return;
    }

    viewer.viewport.setZoom(1);
    const sizer = viewer.shadowRoot.querySelector('#sizer');
    chrome.test.assertEq(826, sizer.offsetWidth);
    chrome.test.assertEq(1066 + viewer.getToolbarHeight(), sizer.offsetHeight);
    chrome.test.succeed();
  },

  function testGetSelectedText() {
    const client = new PDFScriptingAPI(window, window);
    client.selectAll();
    client.getSelectedText(function(selectedText) {
      chrome.test.assertEq('this is some text\nsome more text', selectedText);
      chrome.test.succeed();
    });
  },

  function testGetThumbnail() {
    const client = new PDFScriptingAPI(window, window);
    client.getThumbnail(1, data => {
      const expectedWidth = 108 * window.devicePixelRatio;
      const expectedHeight = 140 * window.devicePixelRatio;
      chrome.test.assertEq(expectedWidth, data.width);
      chrome.test.assertEq(expectedHeight, data.height);

      const expectedByteLength = expectedWidth * expectedHeight * 4;
      chrome.test.assertEq(expectedByteLength, data.imageData.byteLength);

      chrome.test.succeed();
    });
  },

  /**
   * Test that the filename is used as the title.pdf.
   */
  function testHasCorrectTitle() {
    chrome.test.assertEq('test.pdf', document.title);
    chrome.test.succeed();
  },
];

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCompleteCallback(function() {
  chrome.test.runTests(tests);
});
