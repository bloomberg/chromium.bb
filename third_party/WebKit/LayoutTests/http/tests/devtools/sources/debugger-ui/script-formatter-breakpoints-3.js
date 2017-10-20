// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the script formatting is working fine with breakpoints.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('../debugger/resources/unformatted3.js');

  Bindings.breakpointManager._storage._breakpoints = {};
  var panel = UI.panels.sources;
  var scriptFormatter;

  SourcesTestRunner.runDebuggerTestSuite([
    function testSetup(next) {
      SourcesTestRunner.scriptFormatter().then(function(sf) {
        scriptFormatter = sf;
        next();
      });
    },

    function testBreakpointsSetInFormattedAndRemoveInOriginalSource(next) {
      SourcesTestRunner.showScriptSource('unformatted3.js', didShowScriptSource);

      function didShowScriptSource(frame) {
        TestRunner.addSniffer(
            Sources.ScriptFormatterEditorAction.prototype, '_updateButton', uiSourceCodeScriptFormatted);
        scriptFormatter._toggleFormatScriptSource();
      }

      function uiSourceCodeScriptFormatted() {
        var formattedSourceFrame = panel.visibleView;
        SourcesTestRunner.setBreakpoint(formattedSourceFrame, 3, '', true);
        SourcesTestRunner.waitBreakpointSidebarPane().then(evaluateF2);
      }

      function evaluateF2() {
        SourcesTestRunner.waitUntilPaused(pausedInF2);
        TestRunner.evaluateInPageWithTimeout('f2()');
      }


      function pausedInF2(callFrames) {
        SourcesTestRunner.dumpBreakpointSidebarPane('while paused in pretty printed');
        SourcesTestRunner.waitBreakpointSidebarPane()
            .then(() => SourcesTestRunner.dumpBreakpointSidebarPane('while paused in raw'))
            .then(() => SourcesTestRunner.resumeExecution(next));
        // No need to remove breakpoint since formattedUISourceCode was removed.
        Sources.sourceFormatter.discardFormattedUISourceCode(panel.visibleView.uiSourceCode());
      }
    }
  ]);
})();
