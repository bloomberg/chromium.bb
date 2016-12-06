<?php
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that fullscreen feature when disabled may not be called by
// any iframe even when allowfullscreen is set.

Header("Feature-Policy: {\"fullscreen\": []}");
?>

<!DOCTYPE html>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<iframe id="f1"></iframe>
<iframe id="f2" allowfullscreen></iframe>
<script>
var srcs = [
  "resources/feature-policy-fullscreen.html",
  "http://localhost:8000/feature-policy/resources/feature-policy-fullscreen.html"
];
var f1 = document.getElementById('f1');
var f2 = document.getElementById('f2');

function loadFrames(iframe) {
  for (var src of srcs) {
    promise_test(function() {
      iframe.src = src;
      return new Promise(function(resolve, reject) {
        window.addEventListener('message', function(e) {
          resolve(e.data);
        }, { once: true });
      }).then(function(data) {
        assert_false(data.enabled, 'Document.fullscreenEnabled:');
        assert_equals(data.type, 'error', 'Document.requestFullscreen():');
      });
    }, 'Fullscreen disabled on URL: ' + src + ' with allowfullscreen = ' + iframe.allowFullscreen);
  }
}

loadFrames(f1);
loadFrames(f2);
</script>
