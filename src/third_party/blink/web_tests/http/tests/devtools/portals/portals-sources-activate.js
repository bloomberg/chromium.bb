// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(async function () {
  TestRunner.addResult(`Checks that breakpoint set inside a portalactivate event handler is hit on activation`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.navigatePromise('resources/append-predecessor-host.html');

  await SourcesTestRunner.startDebuggerTestPromise();
  const sourceFrame = await SourcesTestRunner.showScriptSourcePromise('append-predecessor.html');
  await SourcesTestRunner.toggleBreakpoint(sourceFrame, 4);
  TestRunner.evaluateInPage(`setTimeout(() => document.querySelector('portal').activate());`);
  const callFrames = await SourcesTestRunner.waitUntilPausedPromise();
  await SourcesTestRunner.captureStackTrace(callFrames);
  SourcesTestRunner.completeDebuggerTest();
})();
