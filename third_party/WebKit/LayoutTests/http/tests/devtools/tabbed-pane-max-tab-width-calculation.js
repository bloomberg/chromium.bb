// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests tabbed pane max tab element width calculation.\n`);
  await TestRunner.loadHTML(`
      <a href="https://bugs.webkit.org/show_bug.cgi?id=75005">Bug 75005</a>
    `);

  function calculateAndDumpMaxWidth(measuredWidths, totalWidth) {
    var maxWidth = UI.TabbedPane.prototype._calculateMaxWidth(measuredWidths, totalWidth);
    TestRunner.addResult(
        'measuredWidths = [' + String(measuredWidths) + '], totalWidth = ' + totalWidth + ', maxWidth = ' + maxWidth +
        '.');
  }

  calculateAndDumpMaxWidth([50, 70, 20], 150);
  calculateAndDumpMaxWidth([50, 80, 20], 150);
  calculateAndDumpMaxWidth([50, 90, 20], 150);
  calculateAndDumpMaxWidth([90, 90, 20], 150);
  calculateAndDumpMaxWidth([90, 80, 70], 150);

  TestRunner.completeTest();
})();
