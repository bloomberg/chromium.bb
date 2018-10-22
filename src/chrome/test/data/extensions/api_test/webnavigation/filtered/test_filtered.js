// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;
    chrome.test.runTests([
      function dontGetEventToWrongUrl() {
        var a_visited = false;
        chrome.webNavigation.onCommitted.addListener(function(details) {
          chrome.test.fail();
        }, { url: [{pathSuffix: 'never-navigated.html'}] });
        chrome.webNavigation.onCommitted.addListener(function(details) {
          chrome.test.assertTrue(details.url == getURL('a.html'));
          a_visited = true;
        }, { url: [{pathSuffix: 'a.html'}] });
        chrome.webNavigation.onCommitted.addListener(function(details) {
          chrome.test.assertTrue(details.url == getURL('b.html'));
          chrome.test.assertTrue(a_visited);
          chrome.test.succeed();
        }, { url: [{pathSuffix: 'b.html'}] });
        chrome.tabs.update(tabId, { url: getURL('a.html') });
      }
    ]);
  });
};
