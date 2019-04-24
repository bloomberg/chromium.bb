// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackFail = chrome.test.callbackFail;

var emptyCallback = function() {};
var expectedError = 'Not implemented';

chrome.test.runTests([
  function addDynamicRules() {
    chrome.declarativeNetRequest.addDynamicRules(
        [], callbackFail(expectedError, emptyCallback));
  },
  function removeDynamicRules() {
    chrome.declarativeNetRequest.removeDynamicRules(
        [], callbackFail(expectedError, emptyCallback));
  },
  function getDynamicRules() {
    chrome.declarativeNetRequest.getDynamicRules(
        callbackFail(expectedError, function(rules) {}));
  }
]);
