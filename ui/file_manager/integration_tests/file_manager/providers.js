// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {
/**
 * Tests if mounting a provided file system via the "Add new services" button
 * works correctly.
 */
function requestMount() {
  var appId;
  StepsRunner.run([
    function() {
      chrome.test.sendMessage(
          JSON.stringify({name: 'installProviderExtension'}));
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    function(inAppId) {
      appId = inAppId;
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick',
          appId,
          ['div[menu=\'#add-new-services-menu\']'],
          this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(
          appId,
          '#add-new-services-menu:not([hidden]) cr-menu-item:first-child span')
          .then(this.next);
    },
    function(result) {
      chrome.test.assertEq('Testing Provider', result.text);
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick',
          appId,
          ['#add-new-services-menu cr-menu-item:first-child span'],
          this.next);
    },
    function(result) {
      remoteCall.waitForElement(
          appId,
          '.tree-row[selected] .icon[volume-type-icon="provided"]')
          .then(this.next);
    },
    function(result) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick',
          appId,
          ['div[menu=\'#add-new-services-menu\']'],
          this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Confirm that the first element of the menu is now the separator, as
      // the extension once mounted should be gone from the menu, as it doesn't
      // support multiple mounts.
      remoteCall.waitForElement(
          appId,
          '#add-new-services-menu:not([hidden]) hr:first-child')
          .then(this.next);
    },
    function(result) {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

// Exports test functions.
testcase.requestMount = requestMount;

})();
