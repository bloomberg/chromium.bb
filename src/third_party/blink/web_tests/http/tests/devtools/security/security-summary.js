// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests specifying a security summary for the Security panel overview.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  /** @type {!Protocol.Security.InsecureContentStatus} */
  var insecureContentStatus = {
    ranMixedContent: false,
    displayedMixedContent: false,
    ranContentWithCertErrors: false,
    displayedContentWithCertErrors: false,
    ranInsecureContentStyle: Protocol.Security.SecurityState.Insecure,
    displayedInsecureContentStyle: Protocol.Security.SecurityState.Neutral
  };
  TestRunner.mainTarget.model(Security.SecurityModel)
      .dispatchEventToListeners(
          Security.SecurityModel.Events.SecurityStateChanged,
          new Security.PageSecurityState(
              Protocol.Security.SecurityState.Secure, true, [], insecureContentStatus, 'Test: Summary Override Text'));

  TestRunner.dumpDeepInnerHTML(
      Security.SecurityPanel._instance()._mainView.contentElement.getElementsByClassName('security-summary-text')[0]);

  TestRunner.completeTest();
})();
