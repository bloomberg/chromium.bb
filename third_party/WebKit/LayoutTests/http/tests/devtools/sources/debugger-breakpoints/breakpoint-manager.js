// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests BreakpointManager class.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  var mockTarget;
  var lastTargetId = 0;

  function resetWorkspace(breakpointManager) {
    mockTarget.debuggerModel.reset();
    TestRunner.addResult('  Resetting workspace.');
    breakpointManager._debuggerWorkspaceBinding._reset(mockTarget.debuggerModel);
    SourcesTestRunner.testNetworkProject._resetForTest();
    SourcesTestRunner.testResourceMappingModelInfo._resetForTest();
  }

  function createBreakpoint(url, lineNumber, condition, enabled) {
    return {url: url, lineNumber: lineNumber, condition: condition, enabled: enabled};
  }

  var serializedBreakpoints = [];
  serializedBreakpoints.push(createBreakpoint('a.js', 10, 'foo == bar', true));
  serializedBreakpoints.push(createBreakpoint('a.js', 20, '', false));
  serializedBreakpoints.push(createBreakpoint('b.js', 3, '', true));

  SourcesTestRunner.setupLiveLocationSniffers();

  function addUISourceCode() {
    var args = [mockTarget].concat(Array.prototype.slice.call(arguments));
    return SourcesTestRunner.addUISourceCode.apply(null, args);
  }

  function createBreakpointManager(serializedBreakpoints) {
    SourcesTestRunner.createWorkspace();
    mockTarget = SourcesTestRunner.createMockTarget(++lastTargetId);
    return SourcesTestRunner.createBreakpointManager(
        SourcesTestRunner.testTargetManager, SourcesTestRunner.testDebuggerWorkspaceBinding, serializedBreakpoints);
  }

  TestRunner.runTestSuite([
    function testBreakpointInCollectedReload(next) {
      var breakpointManager = createBreakpointManager();
      TestRunner.addResult('\n  Adding file without script:');
      var uiSourceCode = addUISourceCode(breakpointManager, 'a.js', true, true);

      TestRunner.addResult('\n  Setting breakpoint:');
      SourcesTestRunner.BreakpointManager.setBreakpoint(breakpointManager, uiSourceCode, 10, 0, '', true, step2);

      function step2() {
        SourcesTestRunner.dumpBreakpointLocations(breakpointManager);
        TestRunner.addResult('\n  Reloading:');
        resetWorkspace(breakpointManager);

        TestRunner.addResult('\n  Adding file with script:');
        var uiSourceCode = addUISourceCode(breakpointManager, 'a.js');

        TestRunner.addResult('\n  Emulating breakpoint resolved event:');
        mockTarget.debuggerModel._breakpointResolved(
            'a.js:10', new SDK.DebuggerModel.Location(mockTarget.debuggerModel, 'a.js', 10, 0));

        TestRunner.addResult('\n  Make sure we don\'t do any unnecessary breakpoint actions:');
        SourcesTestRunner.runAfterPendingBreakpointUpdates(breakpointManager, breakpointActionsPerformed.bind(this));

        function breakpointActionsPerformed() {
          SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
        }
      }
    },
  ]);
})();
