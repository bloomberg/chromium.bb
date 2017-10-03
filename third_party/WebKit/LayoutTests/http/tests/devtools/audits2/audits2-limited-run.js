// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  TestRunner.addResult('Tests that audits panel works when only performance category is selected.\n');

  await TestRunner.loadModule('audits2_test_runner');
  await TestRunner.showPanel('audits2');

  Audits2TestRunner.openDialog();
  var dialogElement = Audits2TestRunner.getDialogElement();
  var checkboxes = dialogElement.querySelectorAll('.checkbox');
  for (var checkbox of checkboxes) {
    if (checkbox.textElement.textContent === 'Performance')
      continue;

    checkbox.checkboxElement.click();
  }

  Audits2TestRunner.dumpDialogState();
  Audits2TestRunner.getRunButton().click();

  var results = await Audits2TestRunner.waitForResults();
  TestRunner.addResult(`\n=============== Lighthouse Results ===============`);

  Object.keys(results.audits).sort().forEach(auditName => {
    var audit = results.audits[auditName];
    TestRunner.addResult(`${audit.name}: ${Boolean(audit.rawValue)}`);
  });

  TestRunner.completeTest();
})();
