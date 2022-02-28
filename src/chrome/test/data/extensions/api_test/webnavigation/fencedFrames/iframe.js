// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.getConfig((config) => {
    const port = config.testServer.port;
    var f = document.createElement('fencedframe');
    f.src = 'http://a.com:' + port +
        '/extensions/api_test/webnavigation/fencedFrames/frame.html';
    document.body.appendChild(f);
  });
};
