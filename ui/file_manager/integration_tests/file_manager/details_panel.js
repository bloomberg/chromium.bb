// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

testcase.openDetailsPanel = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('fakeEvent',
                                    appId,
                                    ['#details-button', 'click'],
                                    this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId, "#details-container").then(this.next);
    },
    function(result) {
      chrome.test.assertFalse(result.hidden);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

testcase.openDetailsPanelForSingleFile = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('fakeEvent',
                                    appId,
                                    ['#details-button', 'click'],
                                    this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId, "#details-container").then(this.next);
    },
    function(result) {
      chrome.test.assertFalse(result.hidden);
      remoteCall.waitForAFile('downloads', 'hello.txt').then(this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId, "#single-file-details").then(this.next);
    },
    function(result) {
      chrome.test.assertFalse(result.hidden);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

testcase.openSingleFileAndSeeDetailsPanel = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('fakeEvent',
                                    appId,
                                    ['#details-button', 'click'],
                                    this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId, "#details-container").then(this.next);
    },
    function(result) {
      chrome.test.assertFalse(result.hidden);
      remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt'],
          this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId, "#single-file-details .filename")
          .then(this.next);
    },
    function(result) {
      chrome.test.assertEq('hello.txt', result.text);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
