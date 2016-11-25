<?php
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that navigator.vibrate when disabled may not be called by
// any iframe.

Header("Feature-Policy: {\"vibrate\": []}");
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
</script>
</head>
<body>
<iframe id="f1" src="resources/feature-policy-vibrate-disabled.html"></iframe>
<iframe id="f2" src="http://localhost:8000/feature-policy/resources/feature-policy-vibrate-disabled.html"></iframe>
</body>
</html>
