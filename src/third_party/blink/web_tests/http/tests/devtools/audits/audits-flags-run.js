// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that audits panel passes flags.\n');

  await TestRunner.loadModule('audits_test_runner');
  await TestRunner.showPanel('audits');

  const dialogElement = AuditsTestRunner.getContainerElement();
  dialogElement.querySelector('input[name="audits.device_type"][value="desktop"]').click();
  dialogElement.querySelector('input[name="audits.throttling"][value="off"]').click();

  AuditsTestRunner.dumpStartAuditState();
  AuditsTestRunner.getRunButton().click();

  const results = await AuditsTestRunner.waitForResults();
  TestRunner.addResult(`\n=============== Lighthouse Results ===============`);
  TestRunner.addResult(`emulatedFormFactor: ${results.configSettings.emulatedFormFactor}`);
  TestRunner.addResult(`disableStorageReset: ${results.configSettings.disableStorageReset}`);
  TestRunner.addResult(`throttlingMethod: ${results.configSettings.throttlingMethod}`);

  TestRunner.completeTest();
})();
