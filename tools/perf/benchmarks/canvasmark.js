// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  if (window.parent != window)  // Ignore subframes.
    return;

  window.__results = [];
  var __real_log = window.console.log;
  window.console.log = function(msg) {
    __results.push(msg);
    __real_log.apply(this, [msg]);
  };
})();
