// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests opening files in the browser using content sniffing.
 */

'use strict';

(() => {
  /**
   * Tests opening a PDF with missing filename extension from Files app.
   *
   * @param {string} path Directory path (Downloads or Drive).
   */
  async function pdfOpen(path) {
    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.imgPdf.targetPath],
      openType: 'launch'
    });

    // Open Files.App on |path|, add imgpdf to Downloads and Drive.
    const appId =
        await setupAndWaitUntilReady(path, [ENTRIES.imgPdf], [ENTRIES.imgPdf]);

    // Open the pdf file from Files app.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, [ENTRIES.imgPdf.targetPath]));
    // Wait for a browser window to appear.
    const browserWindows = await getBrowserWindows();
    // Find the main (normal) browser window.
    let normalWindow = undefined;
    for (let i = 0; i < browserWindows.length; ++i) {
      if (browserWindows[i].type == 'normal') {
        normalWindow = browserWindows[i];
        break;
      }
    }
    // Check we have found a 'normal' browser window.
    chrome.test.assertTrue(normalWindow !== undefined);
    // Check we have only one tab opened from trying to open the file.
    const tabs = normalWindow.tabs;
    chrome.test.assertTrue(tabs.length === 1);
    // Check the end of the URL matches the file we tried to open.
    const tail = tabs[0].url.replace(/.*\//, '');
    chrome.test.assertTrue(tail === ENTRIES.imgPdf.targetPath);
  }

  testcase.pdfOpenDownloads = () => {
    return pdfOpen(RootPath.DOWNLOADS);
  };

  testcase.pdfOpenDrive = () => {
    return pdfOpen(RootPath.DRIVE);
  };
})();
