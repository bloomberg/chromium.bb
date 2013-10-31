// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
if (window.parent != window)  // Ignore subframes.
  return;
document.addEventListener('WebComponentsReady', function() {
  var unused = document.body.offsetHeight;
  window.__pica_load_time = performance.now();
  setTimeout(function() {
    window.__web_components_ready=true;
  }, 1000);
}
);
})();
