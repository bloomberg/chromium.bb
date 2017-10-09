// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Checks that JavaScriptSourceFrame show inline breakpoints correctly\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function foo()
      {
          var p = Promise.resolve().then(() => console.log(42))
              .then(() => console.log(239));
          return p;
      }
      //# sourceURL=foo.js
    `);

  function waitAndDumpDecorations(sourceFrame) {
    return SourcesTestRunner.waitJavaScriptSourceFrameBreakpoints(sourceFrame)
        .then(() => SourcesTestRunner.dumpJavaScriptSourceFrameBreakpoints(sourceFrame));
  }

  Bindings.breakpointManager._storage._breakpoints = {};
  SourcesTestRunner.runDebuggerTestSuite([
    function testAddRemoveBreakpoint(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('foo.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint');
        SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 11, '', true)
            .then(() => waitAndDumpDecorations(javaScriptSourceFrame).then(removeBreakpoint));
      }

      function removeBreakpoint() {
        TestRunner.addResult('Toggle breakpoint');
        waitAndDumpDecorations(javaScriptSourceFrame).then(() => next());
        SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 11);
      }
    },

    function testAddRemoveBreakpointInLineWithOneLocation(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('foo.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint');
        SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 13, '', true)
            .then(() => waitAndDumpDecorations(javaScriptSourceFrame).then(removeBreakpoint));
      }

      function removeBreakpoint() {
        TestRunner.addResult('Toggle breakpoint');
        SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 13);
        waitAndDumpDecorations(javaScriptSourceFrame).then(() => next());
      }
    },

    function clickByInlineBreakpoint(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('foo.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint');
        SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 11, '', true)
            .then(() => waitAndDumpDecorations(javaScriptSourceFrame).then(clickBySecondLocation));
      }

      function clickBySecondLocation() {
        TestRunner.addResult('Click by second breakpoint');
        waitAndDumpDecorations(javaScriptSourceFrame).then(clickByFirstLocation);
        SourcesTestRunner.clickJavaScriptSourceFrameBreakpoint(javaScriptSourceFrame, 11, 1, next);
      }

      function clickByFirstLocation() {
        TestRunner.addResult('Click by first breakpoint');
        waitAndDumpDecorations(javaScriptSourceFrame).then(clickBySecondLocationAgain);
        SourcesTestRunner.clickJavaScriptSourceFrameBreakpoint(javaScriptSourceFrame, 11, 0, next);
      }

      function clickBySecondLocationAgain() {
        TestRunner.addResult('Click by second breakpoint');
        waitAndDumpDecorations(javaScriptSourceFrame).then(() => next());
        SourcesTestRunner.clickJavaScriptSourceFrameBreakpoint(javaScriptSourceFrame, 11, 1, next);
      }
    },

    function toggleBreakpointInAnotherLineWontRemoveExisting(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('foo.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint in line 4');
        SourcesTestRunner.toggleBreakpoint(sourceFrame, 12, false);
        waitAndDumpDecorations(javaScriptSourceFrame).then(toggleBreakpointInAnotherLine);
      }

      function toggleBreakpointInAnotherLine() {
        TestRunner.addResult('Setting breakpoint in line 3');
        waitAndDumpDecorations(javaScriptSourceFrame).then(removeBreakpoints);
        SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 11, false);
      }

      function removeBreakpoints() {
        TestRunner.addResult('Click by first inline breakpoints');
        waitAndDumpDecorations(javaScriptSourceFrame).then(() => next());
        SourcesTestRunner.clickJavaScriptSourceFrameBreakpoint(javaScriptSourceFrame, 11, 0, next);
        SourcesTestRunner.clickJavaScriptSourceFrameBreakpoint(javaScriptSourceFrame, 12, 0, next);
      }
    }
  ]);
})();
