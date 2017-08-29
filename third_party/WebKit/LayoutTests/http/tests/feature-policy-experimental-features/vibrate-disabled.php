<?php
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that navigator.vibrate when disabled may not be called by
// any iframe.

Header("Feature-Policy: vibrate 'none'");
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
        assert_false(data.enabled, 'navigator.vibrate():');
      });
    }, 'Navigator.vibrate disabled on URL: ' + src);
  }
  for (var src of srcs) {
    loadFrame(src);
  }
}
</script>
