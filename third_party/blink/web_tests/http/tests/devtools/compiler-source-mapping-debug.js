// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests installing compiler source map in scripts panel.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.addScriptTag('resources/compiled-2.js');

  await TestRunner.evaluateInPagePromise(`addElements()`);
  await TestRunner.evaluateInPagePromise(`
      function clickButton()
      {
          document.getElementById('test').click();
      }

      function installScriptWithPoorSourceMap()
      {
          var script = document.createElement("script");
          script.setAttribute("src", "resources/compiled-with-wrong-source-map-url.js");
          document.head.appendChild(script);
      }
  `);

  SourcesTestRunner.runDebuggerTestSuite([function testSetBreakpoint(next) {
    SourcesTestRunner.showScriptSource('add-elements.js', didShowSource);

    async function didShowSource(sourceFrame) {
      TestRunner.addResult('Script source was shown.');
      await SourcesTestRunner.setBreakpoint(sourceFrame, 14, '', true);
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPage('setTimeout(clickButton, 0)');
    }

    async function paused(callFrames) {
      await SourcesTestRunner.captureStackTrace(callFrames);
      SourcesTestRunner.resumeExecution(next);
    }
  }]);
})();
