// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function openQuickViewSteps(appId, name) {
  let caller = getCaller();

  function checkQuickViewElementsDisplayBlock(elements) {
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0].styles.display !== 'block')
      return pending(caller, 'Waiting for Quick View to open.');
    return;
  }

  return [
    // Select file |name| in the file list.
    function() {
      remoteCall.callRemoteTestUtil('selectFile', appId, [name], this.next);
    },
    // Press the space key.
    function(result) {
      chrome.test.assertTrue(!!result, 'selectFile failed');
      const space = ['#file-list', ' ', ' ', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space, this.next);
    },
    // Check: the Quick View element should be shown.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeKeyDown failed');
      repeatUntil(function() {
        const elements = ['#quick-view', '#dialog'];
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [elements, null, ['display']])
            .then(checkQuickViewElementsDisplayBlock);
      }).then(this.next);
    },
  ];
}

function closeQuickViewSteps(appId) {
  let caller = getCaller();

  function checkQuickViewElementsDisplayNone(elements) {
    chrome.test.assertTrue(Array.isArray(elements));
    if (elements.length > 0 && elements[0].styles.display !== 'none')
      return pending(caller, 'Waiting for Quick View to close.');
    return;
  }

  return [
    // Click on Quick View to close it.
    function() {
      const panelElements = ['#quick-view', '#contentPanel'];
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [panelElements])
          .then(this.next);
    },
    // Check: the Quick View element should not be shown.
    function(result) {
      chrome.test.assertTrue(!!result, 'fakeMouseClick failed');
      repeatUntil(function() {
        const elements = ['#quick-view', '#dialog'];
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId, [elements, null, ['display']])
            .then(checkQuickViewElementsDisplayNone);
      }).then(this.next);
    },
  ];
}

/**
 * Tests opening Quick View on a local downloads file.
 */
testcase.openQuickView = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on local downloads.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Open a file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

/**
 * Tests opening then closing Quick View on a local downloads file.
 */
testcase.closeQuickView = function() {
  let appId;

  StepsRunner.run([
    // Open Files app on local downloads.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Open a file in Quick View.
    function(results) {
      appId = results.windowId;
      const openSteps = openQuickViewSteps(appId, ENTRIES.hello.nameText);
      StepsRunner.run(openSteps).then(this.next);
    },
    // Close Quick View.
    function() {
      StepsRunner.run(closeQuickViewSteps(appId)).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
