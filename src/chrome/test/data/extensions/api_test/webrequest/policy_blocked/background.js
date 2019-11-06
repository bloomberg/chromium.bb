// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.protectedOriginCount = 0;

// Return messages to tell which URL requests are visible to the extension.
chrome.webRequest.onBeforeRequest.addListener(function(details) {
   if (details.url.indexOf('example2') != -1) {
     ++window.protectedOriginCount;
   }
   if (details.url.indexOf('protected_url') != -1) {
     chrome.test.sendMessage('protected_url');
   }
}, {urls: ['<all_urls>']}, []);

chrome.test.sendMessage('ready');
