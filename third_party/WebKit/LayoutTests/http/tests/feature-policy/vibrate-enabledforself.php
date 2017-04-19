<?php
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that navigator.vibrate when enabled for self only works on
// the same origin.

Header("Feature-Policy: {\"vibrate\": [\"self\"]}");
?>

<!DOCTYPE html>
<html>
<head>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script>
if (window.testRunner) {
  testRunner.dumpAsText();
  testRunner.dumpChildFramesAsText();
}

function loaded() {
  var iframes = document.getElementsByTagName('iframe');
  for (var i = 0; i < iframes.length; ++i) {
    var iframe = iframes[i];
    // The iframe uses eventSender to emulate a user navigatation, which requires absolute coordinates.
    iframe.contentWindow.postMessage({x: iframe.offsetLeft, y: iframe.offsetTop}, "*");
  }
}
</script>
</head>
<body onload="loaded();">
<iframe id="f1" src="resources/feature-policy-vibrate-enabled.html"></iframe>
<iframe id="f2" src="http://localhost:8000/feature-policy/resources/feature-policy-vibrate-disabled.html"></iframe>
</body>
</html>
