// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the cache storage list live updates.\n`);
  await TestRunner.loadModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var cacheStorageModel = TestRunner.mainTarget.model(SDK.ServiceWorkerCacheModel);
  cacheStorageModel.enable();
  cacheStorageModel._throttler._timeout = 0;

  await ApplicationTestRunner.clearAllCaches();
  await ApplicationTestRunner.dumpCacheTree();

  var promise = TestRunner.addSnifferPromise(SDK.ServiceWorkerCacheModel.prototype, '_cacheAdded');
  ApplicationTestRunner.createCache('testCache1');
  await promise;
  await ApplicationTestRunner.dumpCacheTreeNoRefresh();

  promise = TestRunner.addSnifferPromise(SDK.ServiceWorkerCacheModel.prototype, '_cacheAdded');
  ApplicationTestRunner.createCache('testCache2');
  await promise;
  await ApplicationTestRunner.dumpCacheTreeNoRefresh();

  promise = TestRunner.addSnifferPromise(SDK.ServiceWorkerCacheModel.prototype, '_cacheRemoved');
  ApplicationTestRunner.deleteCache('testCache1');
  await promise;
  await ApplicationTestRunner.dumpCacheTreeNoRefresh();

  await ApplicationTestRunner.clearAllCaches();
  TestRunner.completeTest();
})();
