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
    function testSetBreakpoint(next) {
      var breakpointManager = createBreakpointManager();
      var uiSourceCode = addUISourceCode(breakpointManager, 'a.js');
      SourcesTestRunner.BreakpointManager.setBreakpoint(breakpointManager, uiSourceCode, 30, 0, '', true);
      SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
    },

    function testSetDisabledBreakpoint(next) {
      var breakpointManager = createBreakpointManager();
      var uiSourceCode = addUISourceCode(breakpointManager, 'a.js');
      var breakpoint = SourcesTestRunner.BreakpointManager.setBreakpoint(breakpointManager, uiSourceCode, 30, 0, '', false);
      SourcesTestRunner.dumpBreakpointLocations(breakpointManager);
      SourcesTestRunner.dumpBreakpointStorage(breakpointManager);
      TestRunner.addResult('  Enabling breakpoint');
      breakpoint.setEnabled(true);
      SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
    },

    function testSetConditionalBreakpoint(next) {
      var breakpointManager = createBreakpointManager();
      var uiSourceCode = addUISourceCode(breakpointManager, 'a.js');
      var breakpoint =
          SourcesTestRunner.BreakpointManager.setBreakpoint(breakpointManager, uiSourceCode, 30, 0, 'condition', true, step2);

      function step2() {
        SourcesTestRunner.dumpBreakpointLocations(breakpointManager);
        SourcesTestRunner.dumpBreakpointStorage(breakpointManager);
        TestRunner.addResult('  Updating condition');
        breakpoint.setCondition('');
        SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
      }
    },

    function testRestoreBreakpoints(next) {
      var breakpointManager = createBreakpointManager(serializedBreakpoints);
      addUISourceCode(breakpointManager, 'a.js');
      SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
    },

    function testRestoreBreakpointsTwice(next) {
      var breakpointManager = createBreakpointManager(serializedBreakpoints);
      addUISourceCode(breakpointManager, 'a.js');
      addUISourceCode(breakpointManager, 'a.js');
      SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
    },

    function testRemoveBreakpoints(next) {
      var breakpointManager = createBreakpointManager(serializedBreakpoints);
      var uiSourceCode = addUISourceCode(breakpointManager, 'a.js');
      window.setBreakpointCallback = step2.bind(this);

      function step2() {
        SourcesTestRunner.dumpBreakpointLocations(breakpointManager);
        SourcesTestRunner.BreakpointManager.setBreakpoint(breakpointManager, uiSourceCode, 30, 0, '', true, step3);
      }

      function step3() {
        SourcesTestRunner.dumpBreakpointLocations(breakpointManager);
        SourcesTestRunner.BreakpointManager.removeBreakpoint(breakpointManager, uiSourceCode, 30, 0);
        SourcesTestRunner.BreakpointManager.removeBreakpoint(breakpointManager, uiSourceCode, 10, 0);
        SourcesTestRunner.BreakpointManager.removeBreakpoint(breakpointManager, uiSourceCode, 20, 0);
        SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
      }
    },

    function testSetBreakpointThatShifts(next) {
      var breakpointManager = createBreakpointManager();
      var uiSourceCode = addUISourceCode(breakpointManager, 'a.js');
      SourcesTestRunner.BreakpointManager.setBreakpoint(breakpointManager, uiSourceCode, 1015, 0, '', true);
      SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
    },

    function testSetBreakpointThatShiftsTwice(next) {
      var breakpointManager = createBreakpointManager();
      var uiSourceCode = addUISourceCode(breakpointManager, 'a.js');
      SourcesTestRunner.BreakpointManager.setBreakpoint(breakpointManager, uiSourceCode, 1015, 0, '', true, step2);

      function step2() {
        SourcesTestRunner.dumpBreakpointLocations(breakpointManager);
        SourcesTestRunner.BreakpointManager.setBreakpoint(breakpointManager, uiSourceCode, 1015, 0, '', true);
        SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
      }
    },

    function testSetBreakpointOutsideScript(next) {
      var breakpointManager = createBreakpointManager();
      var uiSourceCode = addUISourceCode(breakpointManager, 'a.js');
      breakpointManager.setBreakpoint(uiSourceCode, 2500, 0, '', true);
      SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
    },

    function testNavigation(next) {
      var breakpointManager = createBreakpointManager(serializedBreakpoints);
      var uiSourceCodeA = addUISourceCode(breakpointManager, 'a.js');
      window.setBreakpointCallback = step2.bind(this);

      function step2() {
        SourcesTestRunner.dumpBreakpointLocations(breakpointManager);
        TestRunner.addResult('\n  Navigating to B.');
        resetWorkspace(breakpointManager);
        var uiSourceCodeB = addUISourceCode(breakpointManager, 'b.js');
        window.setBreakpointCallback = step3.bind(this);
      }

      function step3() {
        SourcesTestRunner.dumpBreakpointLocations(breakpointManager);
        TestRunner.addResult('\n  Navigating back to A.');
        resetWorkspace(breakpointManager);
        TestRunner.addResult('  Resolving provisional breakpoint.');
        SourcesTestRunner.addScript(mockTarget, breakpointManager, 'a.js');
        mockTarget.debuggerModel._breakpointResolved(
            'a.js:10', new SDK.DebuggerModel.Location(mockTarget.debuggerModel, 'a.js', 10, 0));
        addUISourceCode(breakpointManager, 'a.js', false, true);
        SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
      }
    },

    function testProvisionalBreakpointsResolve(next) {
      var serializedBreakpoints = [];
      serializedBreakpoints.push(createBreakpoint('a.js', 10, 'foo == bar', true));

      var breakpointManager = createBreakpointManager(serializedBreakpoints);
      var uiSourceCode = addUISourceCode(breakpointManager, 'a.js');
      window.setBreakpointCallback = step2.bind(this);

      function step2() {
        SourcesTestRunner.dumpBreakpointLocations(breakpointManager);
        resetWorkspace(breakpointManager);
        TestRunner.addResult('  Resolving provisional breakpoint.');
        SourcesTestRunner.addScript(mockTarget, breakpointManager, 'a.js');
        mockTarget.debuggerModel._breakpointResolved(
            'a.js:10', new SDK.DebuggerModel.Location(mockTarget.debuggerModel, 'a.js', 11, 0));
        var breakpoints = breakpointManager._allBreakpoints();
        TestRunner.assertEquals(
            1, breakpoints.length, 'Exactly one provisional breakpoint should be registered in breakpoint manager.');
        SourcesTestRunner.finishBreakpointTest(breakpointManager, next);
      }
    },

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
