// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;

    chrome.test.runTests([
      // History manipulation via window.history
      function history() {
        expect([
          { label: "a-onBeforeNavigate",
            event: "onBeforeNavigate",
            details: { frameId: 0,
                       parentFrameId: -1,
                       processId: -1,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('a.html') }},
          { label: "a-onCommitted",
            event: "onCommitted",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       transitionQualifiers: [],
                       transitionType: "link",
                       url: getURL('a.html') }},
          { label: "a-onDOMContentLoaded",
            event: "onDOMContentLoaded",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('a.html') }},
          { label: "a-onCompleted",
            event: "onCompleted",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('b.html') }},
          { label: "a-onHistoryStateUpdated",
            event: "onHistoryStateUpdated",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       transitionQualifiers: [],
                       transitionType: "link",
                       url: getURL('b.html') }}],
          [ navigationOrder("a-") ]);
        chrome.tabs.update(tabId, { url: getURL('a.html') });
      },

      // Manipulating history before parsing completed.
      function historyBeforeParsing() {
        expect([
          { label: "a-onBeforeNavigate",
            event: "onBeforeNavigate",
            details: { frameId: 0,
                       parentFrameId: -1,
                       processId: -1,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('c.html') }},
          { label: "a-onCommitted",
            event: "onCommitted",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       transitionQualifiers: [],
                       transitionType: "link",
                       url: getURL('c.html') }},
          { label: "a-onHistoryStateUpdated",
            event: "onHistoryStateUpdated",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       transitionQualifiers: [],
                       transitionType: "link",
                       url: getURL('d.html') }},
          { label: "a-onDOMContentLoaded",
            event: "onDOMContentLoaded",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('d.html') }},
          { label: "a-onCompleted",
            event: "onCompleted",
            details: { frameId: 0,
                       processId: 0,
                       tabId: 0,
                       timeStamp: 0,
                       url: getURL('d.html') }}],
          [ navigationOrder("a-"),
            [ 'a-onCommitted',
              'a-onHistoryStateUpdated',
              'a-onDOMContentLoaded']]);
        chrome.tabs.update(tabId, { url: getURL('c.html') });
      },
    ]);
  });
};
