// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests resolving this object name via source maps.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/resolve-this.js');

  SourcesTestRunner.waitForScriptSource('resolve-this.ts', onSourceMapLoaded);

  function onSourceMapLoaded() {
    SourcesTestRunner.startDebuggerTest(() => SourcesTestRunner.runTestFunctionAndWaitUntilPaused());
    TestRunner.addSniffer(
        Sources.ScopeChainSidebarPane.prototype, '_sidebarPaneUpdatedForTest', onSidebarRendered, true);
  }

  function onSidebarRendered() {
    SourcesTestRunner.expandScopeVariablesSidebarPane(onSidebarsExpanded);
  }

  function onSidebarsExpanded() {
    TestRunner.addResult('');
    SourcesTestRunner.dumpScopeVariablesSidebarPane();
    SourcesTestRunner.completeDebuggerTest();
  }
})();
