// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that workers are correctly detached upon navigation.\n`);
  TestRunner.printDevToolsConsole();

  var workerTargetId;
  var navigated = false;
  var observer = {
    targetAdded(target) {
      if (!TestRunner.isDedicatedWorker(target))
        return;
      TestRunner.addResult('Worker added');
      workerTargetId = target.id();
      if (navigated)
        TestRunner.completeTest();
    },
    targetRemoved(target) {
      if (!TestRunner.isDedicatedWorker(target))
        return;
      if (target.id() === workerTargetId) {
        TestRunner.addResult('Worker removed');
        workerTargetId = '';
      } else {
        TestRunner.addResult('Unknown worker removed');
      }
    }
  };
  await TestRunner.navigatePromise('resources/workers-on-navigation-resource.html');

  SDK.targetManager.observeTargets(observer);
  await TestRunner.evaluateInPagePromise('startWorker()');
  await TestRunner.reloadPagePromise();
  navigated = true;
  await TestRunner.evaluateInPagePromise('startWorker()');
})();
