// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that the sidebar origin list disappears and appers when an interstitial is shown or hidden.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  var request1 = new SDK.NetworkRequest(0, 'https://foo.test/', 'https://foo.test', 0, 0, null);
  request1.setSecurityState(Protocol.Security.SecurityState.Secure);
  SecurityTestRunner.dispatchRequestFinished(request1);

  var request2 = new SDK.NetworkRequest(0, 'https://bar.test/foo.jpg', 'https://bar.test', 0, 0, null);
  request2.setSecurityState(Protocol.Security.SecurityState.Secure);
  SecurityTestRunner.dispatchRequestFinished(request2);

  TestRunner.addResult('Before interstitial is shown:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel._instance()._sidebarTree.element);

  // Test that the sidebar is hidden when an interstitial is shown. https://crbug.com/559150
  TestRunner.mainTarget.model(SDK.ResourceTreeModel)
      .dispatchEventToListeners(SDK.ResourceTreeModel.Events.InterstitialShown);
  // Simulate a request finishing after the interstitial is shown, to make sure that doesn't show up in the sidebar.
  var request3 = new SDK.NetworkRequest(0, 'https://bar.test/foo.jpg', 'https://bar.test', 0, 0, null);
  request3.setSecurityState(Protocol.Security.SecurityState.Unknown);
  SecurityTestRunner.dispatchRequestFinished(request3);
  TestRunner.addResult('After interstitial is shown:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel._instance()._sidebarTree.element);

  // Test that the sidebar is shown again when the interstitial is hidden. https://crbug.com/559150
  TestRunner.mainTarget.model(SDK.ResourceTreeModel)
      .dispatchEventToListeners(SDK.ResourceTreeModel.Events.InterstitialHidden);
  TestRunner.addResult('After interstitial is hidden:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel._instance()._sidebarTree.element);

  TestRunner.completeTest();
})();
