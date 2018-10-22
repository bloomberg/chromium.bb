// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that requests to unresolved origins result in unknown security state and show up in the sidebar origin list.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  var request = new SDK.NetworkRequest(0, 'http://unknown', 'https://foo.test', 0, 0, null);
  SecurityTestRunner.dispatchRequestFinished(request);

  SecurityTestRunner.dumpSecurityPanelSidebarOrigins();

  TestRunner.completeTest();
})();
