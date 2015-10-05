// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
function __set_cookie(name, value) {
  document.cookie = name + "=" + value + "; path=/";
}

function __onbeforeunload() {
  // Call GC twice to cleanup JS heap before starting a new test.
  if (window.gc) {
    window.gc();
    window.gc();
  }
}

if (window.parent == window) {  // Ignore subframes.
  addEventListener("beforeunload", __onbeforeunload);
}
})();
