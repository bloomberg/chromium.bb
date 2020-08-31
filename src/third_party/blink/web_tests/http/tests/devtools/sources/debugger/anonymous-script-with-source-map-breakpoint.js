// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests breakpoint in anonymous script with source map on reload.`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await SourcesTestRunner.startDebuggerTestPromise();
  TestRunner.navigatePromise(
      'resources/anonymous-script-with-source-map-breakpoint.html');
  await SourcesTestRunner.waitUntilPausedPromise();
  SourcesTestRunner.resumeExecution();
  await SourcesTestRunner.waitUntilResumedPromise();
  let sourceFrame =
      await SourcesTestRunner.showScriptSourcePromise('example.ts');
  await SourcesTestRunner.setBreakpoint(sourceFrame, 0, '', true);
  TestRunner.reloadPage();
  SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(
      () => SourcesTestRunner.completeDebuggerTest());
})();
