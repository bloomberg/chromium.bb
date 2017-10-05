// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  TestRunner.addResult('Tests that audits panel works.\n');

  await TestRunner.loadModule('audits2_test_runner');
  await TestRunner.showPanel('audits2');

  Audits2TestRunner.openDialog();
  Audits2TestRunner.dumpDialogState();

  TestRunner.addResult(`\n=============== Lighthouse Status Updates ===============`);
  Audits2TestRunner.addStatusListener(msg => TestRunner.addResult(msg));
  Audits2TestRunner.getRunButton().click();

  var results = await Audits2TestRunner.waitForResults();
  TestRunner.addResult(`\n=============== Lighthouse Results ===============`);
  TestRunner.addResult(`URL: ${results.url}`);
  TestRunner.addResult(`Version: ${results.lighthouseVersion}`);
  TestRunner.addResult('\n');

  Object.keys(results.audits).sort().forEach(auditName => {
    var audit = results.audits[auditName];
    TestRunner.addResult(`${audit.name}: ${Boolean(audit.rawValue)}`);
  });

  TestRunner.completeTest();
})();
