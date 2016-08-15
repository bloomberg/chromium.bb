// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests opening the Quick View.
 */
testcase.openQuickView = function() {
  var appId;

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    function(results) {
      appId = results.windowId;
      // Select an image file.
      remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['My Desktop Background.png'], this.next);
    },
    function(results) {
      chrome.test.assertTrue(results);
      // Press Space key.
      remoteCall.callRemoteTestUtil(
          'fakeKeyDown', appId,
          ['#file-list', ' ', ' ', false, false, false], this.next);
    },
    function(results) {
      chrome.test.assertTrue(results);

      // Wait until Quick View is displayed.
      repeatUntil(function() {
        return remoteCall
            .callRemoteTestUtil(
                'deepQueryAllElements', appId,
                [['#quick-view', '#dialog'], null, ['display']])
            .then(function(results) {
              chrome.test.assertEq(1, results.length);
              if (results[0].styles.display === 'none') {
                return pending('Quick View is not opened yet.');
              };
              return results;
            });
      }).then(this.next);
    },
    function(results) {
      chrome.test.assertEq(1, results.length);
      // Check Quick View dialog is displayed.
      chrome.test.assertEq('block', results[0].styles.display);

      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
