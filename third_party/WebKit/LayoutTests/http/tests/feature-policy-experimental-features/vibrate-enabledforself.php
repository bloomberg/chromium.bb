<?php
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that navigator.vibrate when enabled for self only works on
// the same origin.

Header("Feature-Policy: vibrate 'self'");
?>

<!DOCTYPE html>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script src="resources/helper.js"></script>
<iframe></iframe>
<script>
var srcs = [
  "resources/feature-policy-vibrate.html",
  "http://localhost:8000/feature-policy-experimental-features/resources/feature-policy-vibrate.html"];

window.onload = function () {
  var iframe = document.querySelector('iframe');
  iframe.addEventListener('load', sendClick);
  function loadFrame(src) {
    promise_test(function() {
      iframe.src = src;
      return new Promise(function(resolve, reject) {
        window.addEventListener('message', function(e) {
          if (e.data.type === 'result') {
            resolve(e.data);
          }
        });
      }).then(function(data) {
        // vibrate is only enabled within the same origin.
        if (src === srcs[0]) {
          assert_true(data.enabled, 'navigator.vibrate():');
        } else {
          assert_false(data.enabled, 'navigator.vibrate():');
        }
      });
    }, 'Navigator.vibrate enabled for self on URL: ' + src);
  }
  for (var src of srcs) {
    loadFrame(src);
  }
}
</script>
