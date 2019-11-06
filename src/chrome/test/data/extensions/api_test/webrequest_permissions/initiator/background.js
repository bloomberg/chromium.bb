// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var requestsIntercepted = 0;

chrome.webRequest.onBeforeRequest.addListener((details) => {
  requestsIntercepted++;
  chrome.test.sendMessage(details.initiator);
}, {urls: ['*://*/extensions/api_test/webrequest/xhr/data.json']}, []);

chrome.test.sendMessage('ready');
