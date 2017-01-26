<?php
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that fullscreen feature when enabled for all works across
// origins regardless of whether allowfullscreen is set. (Feature policy header
// takes precedence over the absence of allowfullscreen.)

Header("Feature-Policy: {\"fullscreen\": [\"*\"]}");
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
    promise_test(function(t) {
      iframe.src = src;
      return new Promise(function(resolve, reject) {
        window.addEventListener('message', function(e) {
          resolve(e.data);
        }, { once: true });
      }).then(function(data) {
        assert_true(data.enabled, 'Document.fullscreenEnabled:');
        assert_equals(data.type, 'change', 'Document.requestFullscreen():');
      });
    }, 'Fullscreen enabled for all on URL: ' + src + ' with allowfullscreen = ' + iframe.allowFullscreen);
  }
}

loadFrames(f1);
loadFrames(f2);
</script>
