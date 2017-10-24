// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that selector line is computed correctly regardless of its start column. Bug 110732.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <style>
      #inspected
      {
        color: green;
      }
      </style>
      <p>
      Tests that selector line is computed correctly regardless of its start column. <a href="https://bugs.webkit.org/show_bug.cgi?id=110732">Bug 110732</a>.
      </p>

      <div id="container">
          <div id="inspected">Text</div>
      </div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/selector-line.css');

  SourcesTestRunner.waitForScriptSource('selector-line.scss', onSourceMapLoaded);

  function onSourceMapLoaded() {
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);
  }

  function step1() {
    ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
