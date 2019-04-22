// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = async function() {
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});
  let config = await promise(chrome.test.getConfig);
  let port = config.testServer.port;

  var URL_LOAD = "http://127.0.0.1:" + port +
    "/extensions/api_test/webnavigation/targetBlank/a.html";
  var URL_TARGET = "http://127.0.0.1:" + port +
    "/extensions/api_test/webnavigation/targetBlank/b.html";

  chrome.test.runTests([
    // Opens a tab and waits for the user to click on a link with
    // target=_blank in it.
    function targetBlank() {
      expect([
        { label: "a-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_LOAD }},
        { label: "a-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: URL_LOAD }},
        { label: "a-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_LOAD }},
        { label: "a-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_LOAD }},
        { label: "b-onCreatedNavigationTarget",
          event: "onCreatedNavigationTarget",
          details: { sourceFrameId: 0,
                     sourceProcessId: 0,
                     sourceTabId: 0,
                     tabId: 1,
                     timeStamp: 0,
                     url: URL_TARGET }},
        { label: "b-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 1,
                     timeStamp: 0,
                     url: URL_TARGET }},
        { label: "b-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 1,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: URL_TARGET }},
        { label: "b-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 1,
                     timeStamp: 0,
                     url: URL_TARGET }},
        { label: "b-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 1,
                     timeStamp: 0,
                     url: URL_TARGET }}],
        [ navigationOrder("a-"),
          navigationOrder("b-"),
          [ "a-onDOMContentLoaded",
            "b-onCreatedNavigationTarget",
            "b-onBeforeNavigate" ]]);

      // Notify the api test that we're waiting for the user.
      chrome.test.notifyPass();
    },
  ]);
};
