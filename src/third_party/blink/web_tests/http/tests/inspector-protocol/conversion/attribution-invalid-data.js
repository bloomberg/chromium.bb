// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that clicking an attribution link in insecure contexts triggers an issue.`);

  await dp.Audits.enable();
  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/empty.html');

  const issuePromises = [dp.Audits.onceIssueAdded(), dp.Audits.onceIssueAdded()];
  await page.loadHTML(`
    <!DOCTYPE html>
    <img src="https://devtools.test:8443/inspector-protocol/conversion/resources/conversion-redirect-invalid-data.php"></img>
    <img src="https://devtools.test:8443/inspector-protocol/conversion/resources/conversion-redirect-missing-data.php"></img>`);
  const issues = await Promise.all(issuePromises);
  testRunner.log(issues[0].params.issue, "Issue reported: ", ['requestId']);
  testRunner.log(issues[1].params.issue, "Issue reported: ", ['requestId']);

  testRunner.completeTest();
})
