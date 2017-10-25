// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that source data are extracted correctly from external stylesheets in UTF-8 with BOM. Bug 59322.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p>
      Tests that source data are extracted correctly from external stylesheets in UTF-8 with BOM. <a href="https://bugs.webkit.org/show_bug.cgi?id=59322">Bug 59322</a>.
      </p>
      <h1 id="inspected">
      I'm red.
      </h1>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/parse-utf8-bom-main.css');

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  function step1() {
    ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
