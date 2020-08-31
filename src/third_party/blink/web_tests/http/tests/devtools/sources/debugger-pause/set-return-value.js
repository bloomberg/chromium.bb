// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Check that return value can be changed.');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
    function testFunction() {
      Promise.resolve(42).then(x => x).then(console.log);
    }
    //# sourceURL=test.js
  `);
  await SourcesTestRunner.startDebuggerTestPromise();
  await TestRunner.DebuggerAgent.invoke_setBreakpointByUrl({lineNumber: 11, url: 'test.js', columnNumber: 37});
  let sidebarUpdated = TestRunner.addSnifferPromise(
        Sources.ScopeChainSidebarPane.prototype, '_sidebarPaneUpdatedForTest');
  await Promise.all([SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise(), sidebarUpdated]);
  let localScope = SourcesTestRunner.scopeChainSections()[0];

  TestRunner.addResult('Dump current');
  await new Promise(resolve => SourcesTestRunner.expandProperties([localScope, ['Return value']], resolve));
  SourcesTestRunner.dumpScopeVariablesSidebarPane();

  TestRunner.addResult('Set return value to {a:1}');
  let returnValueElement = localScope.children().find(x => x.property.name === 'Return value');
  await returnValueElement._applyExpression('{a:1}');
  await new Promise(resolve => SourcesTestRunner.expandProperties([localScope, ['Return value']], resolve));
  SourcesTestRunner.dumpScopeVariablesSidebarPane();

  TestRunner.addResult('Try to remove return value');
  returnValueElement = localScope.children().find(x => x.property.name === 'Return value');
  await returnValueElement._applyExpression('');
  await new Promise(resolve => SourcesTestRunner.expandProperties([localScope, ['Return value']], resolve));
  SourcesTestRunner.dumpScopeVariablesSidebarPane();

  TestRunner.addResult('Set return value to 239');
  returnValueElement = localScope.children().find(x => x.property.name === 'Return value');
  await returnValueElement._applyExpression('239');
  await new Promise(resolve => SourcesTestRunner.expandProperties([localScope, ['Return value']], resolve));
  SourcesTestRunner.dumpScopeVariablesSidebarPane();

  SourcesTestRunner.resumeExecution();
  await ConsoleTestRunner.waitUntilMessageReceivedPromise();
  TestRunner.addResult('Actual return value:');
  await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();

  SourcesTestRunner.completeDebuggerTest();
})();
