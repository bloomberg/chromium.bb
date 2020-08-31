// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Testing accessibility in the threads sidebar pane.');

  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true);

  await TestRunner.evaluateInPagePromise(`new Worker('../../sources/resources/worker-source.js')`);
  await SourcesTestRunner.waitUntilPausedPromise();
  const sourcesPanel = UI.panels.sources;
  sourcesPanel._showThreadsIfNeeded();

  const threadsSidebarPane = await sourcesPanel._threadsSidebarPane.widget();
  const threadsSidebarElement = threadsSidebarPane.contentElement;
  TestRunner.addResult(`Threads sidebar pane content:\n ${threadsSidebarElement.deepTextContent()}`);
  TestRunner.addResult('Running the axe-core linter on the threads sidebar pane.');
  await AxeCoreTestRunner.runValidation(threadsSidebarElement);
  TestRunner.completeTest();

})();
