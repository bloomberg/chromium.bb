// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests Debugger.setBreakpointByUrl with isRegex set to true.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          foo();
      }

      function foo()
      {
      }
  `);

  SourcesTestRunner.runDebuggerTestSuite([
    async function testSetNoneOfURLAndRegex(next) {
      var response = await TestRunner.DebuggerAgent.invoke_setBreakpointByUrl({lineNumber: 1});
      TestRunner.addResult(response[Protocol.InspectorBackend.ProtocolError]);
      next();
    },

    async function testSetBothURLAndRegex(next) {
      var url = 'debugger-set-breakpoint.js';
      var urlRegex = 'debugger-set-breakpoint.*';
      var response = await TestRunner.DebuggerAgent.invoke_setBreakpointByUrl({lineNumber: 1, url, urlRegex});
      TestRunner.addResult(response[Protocol.InspectorBackend.ProtocolError]);
      next();
    },

    async function testSetByRegex(next) {
      await TestRunner.DebuggerAgent.invoke_setBreakpointByUrl({urlRegex: 'debugger-set-breakpoint.*', lineNumber: 11});
      SourcesTestRunner.runTestFunctionAndWaitUntilPaused(async callFrames => {
        await SourcesTestRunner.captureStackTrace(callFrames);
        next();
      });
    }
  ]);
})();
