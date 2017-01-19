<?php
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that payment feature when enabled for self works in
// the same origin or when allowpaymentrequest is set. No cross-origin iframe
// may call it when allowpaymentrequest is not set.

Header("Feature-Policy: {\"payment\": [\"self\"]}");
?>

<!DOCTYPE html>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script>
  if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
  }
</script>
<iframe id="f1" src="resources/feature-policy-payment-enabled.html"></iframe>
<iframe id="f2" src="http://localhost:8000/feature-policy/resources/feature-policy-payment-disabled.html"></iframe>
<iframe id="f3" src="resources/feature-policy-payment-enabled.html" allowpaymentrequest></iframe>
<iframe id="f4" src="http://localhost:8000/feature-policy/resources/feature-policy-payment-enabled.html" allowpaymentrequest></iframe>
