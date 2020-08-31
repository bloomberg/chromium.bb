// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that the sidebar uses the correct styling for mixed content subresources.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  const pageVisibleSecurityState = new Security.PageVisibleSecurityState(
    Protocol.Security.SecurityState.Neutral, null, null,
    ['displayed-mixed-content', 'ran-mixed-content']);
  TestRunner.mainTarget.model(Security.SecurityModel)
      .dispatchEventToListeners(
        Security.SecurityModel.Events.VisibleSecurityStateChanged,
        pageVisibleSecurityState);

  var passive = new SDK.NetworkRequest(0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  passive.mixedContentType = 'optionally-blockable';
  SecurityTestRunner.dispatchRequestFinished(passive);

  var active = new SDK.NetworkRequest(0, 'http://bar.test', 'https://bar.test', 0, 0, null);
  active.mixedContentType = 'blockable';
  SecurityTestRunner.dispatchRequestFinished(active);

  TestRunner.addResult('Origin sidebar:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel._instance()._sidebarTree.element);

  TestRunner.completeTest();
})();
