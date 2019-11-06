// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that audits panel works when only performance category is selected.\n');

  await TestRunner.loadModule('audits_test_runner');
  await TestRunner.showPanel('audits');

  var dialogElement = AuditsTestRunner.getContainerElement();
  var checkboxes = dialogElement.querySelectorAll('.checkbox');
  for (var checkbox of checkboxes) {
    if (checkbox.textElement.textContent === 'Performance' ||
        checkbox.textElement.textContent === 'Clear storage')
      continue;

    checkbox.checkboxElement.click();
  }

  AuditsTestRunner.dumpStartAuditState();
  AuditsTestRunner.getRunButton().click();

  var results = await AuditsTestRunner.waitForResults();
  TestRunner.addResult(`\n=============== Audits run ===============`);
  TestRunner.addResult(Object.keys(results.audits).sort().join('\n'));

  TestRunner.completeTest();
})();
