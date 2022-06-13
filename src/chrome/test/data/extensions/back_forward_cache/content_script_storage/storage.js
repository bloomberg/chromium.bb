// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.storage.onChanged.addListener((changes, area) => {
  // do nothing
});

if (window.location.host.indexOf('b.com') != -1) {
  var options = {test: true};
  chrome.storage.sync.set({options});
}
