// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function isApiAvailable() {
  return chrome && chrome.hasOwnProperty('loginScreenUi') &&
      chrome.loginScreenUi.hasOwnProperty('show') &&
      chrome.loginScreenUi.hasOwnProperty('close') &&
      typeof chrome.loginScreenUi.show === 'function' &&
      typeof chrome.loginScreenUi.close === 'function';
}

var availableTests = [

  function apiAvailable() {
    chrome.test.assertTrue(isApiAvailable());
    chrome.test.succeed();
  },

  function apiNotAvailable() {
    chrome.test.assertFalse(isApiAvailable());
    chrome.test.succeed();
  },

];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));
