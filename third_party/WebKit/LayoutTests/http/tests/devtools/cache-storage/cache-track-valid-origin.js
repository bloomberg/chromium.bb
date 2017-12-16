// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that cache storage live update only tracks valid security origins.\n`);
  await TestRunner.loadModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var cacheStorageModel = TestRunner.mainTarget.model(SDK.ServiceWorkerCacheModel);
  let invalidOrigins = ['http', 'test://fake', 'test://fake.origin.com', 'chrome://test'];
  let validOrigins = ['http://fake.origin.com', 'https://fake.origin.com'];

  TestRunner.addResult('Invalid Origins:');
  invalidOrigins.map(origin => {
    TestRunner.addResult(origin + ', valid = ' + cacheStorageModel._isValidSecurityOrigin(origin));
  });
  TestRunner.addResult('\nValid Origins:');
  validOrigins.map(origin => {
    TestRunner.addResult(origin + ', valid = ' + cacheStorageModel._isValidSecurityOrigin(origin));
  });
  TestRunner.completeTest();
})();
