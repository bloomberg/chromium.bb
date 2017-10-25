// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that CSSParser correctly parses declarations with unterminated strings. Blink bug 231127\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p>
      Tests that CSSParser correctly parses declarations with unterminated strings. <a href="https://code.google.com/p/chromium/issues/detail?id=231127">Blink bug 231127</a>

      </p><div id="inspected" style="color: red'foo"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', dumpStyles);

  function dumpStyles() {
    ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
