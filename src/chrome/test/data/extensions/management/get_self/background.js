//  Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.management.getSelf(function(info) {
  chrome.test.assertTrue(info != null);
  chrome.test.assertEq("Self Get Test (no permissions)", info.name);
  chrome.test.assertEq("extension", info.type);
  chrome.test.assertEq(true, info.enabled);
  chrome.test.sendMessage("success");
});
