// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that database names are correctly loaded and saved in IndexedDBModel.\n`);
  await TestRunner.loadModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  var indexedDBModel = ApplicationTestRunner.createIndexedDBModel();
  var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;

  function dumpDatabaseNames() {
    TestRunner.addResult('Dumping database names:');
    var securityOrigins = TestRunner.securityOriginManager.securityOrigins();
    var securityOrigin = securityOrigins[0];
    var names = indexedDBModel._databaseNamesBySecurityOrigin[securityOrigin];
    for (var i = 0; i < names.length; ++i)
      TestRunner.addResult('    ' + names[i]);
    TestRunner.addResult('');
  }

  TestRunner.addSniffer(Resources.IndexedDBModel.prototype, '_updateOriginDatabaseNames', step2, false);

  function step2() {
    dumpDatabaseNames();
    ApplicationTestRunner.createDatabase(mainFrameId, 'testDatabase1', step3);
  }

  function step3() {
    TestRunner.addSniffer(Resources.IndexedDBModel.prototype, '_updateOriginDatabaseNames', step4, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step4() {
    dumpDatabaseNames();
    ApplicationTestRunner.createDatabase(mainFrameId, 'testDatabase2', step5);
  }

  function step5() {
    TestRunner.addSniffer(Resources.IndexedDBModel.prototype, '_updateOriginDatabaseNames', step6, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step6() {
    dumpDatabaseNames();
    ApplicationTestRunner.deleteDatabase(mainFrameId, 'testDatabase2', step7);
  }

  function step7() {
    TestRunner.addSniffer(Resources.IndexedDBModel.prototype, '_updateOriginDatabaseNames', step8, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step8() {
    dumpDatabaseNames();
    ApplicationTestRunner.deleteDatabase(mainFrameId, 'testDatabase1', step9);
  }

  function step9() {
    TestRunner.addSniffer(Resources.IndexedDBModel.prototype, '_updateOriginDatabaseNames', step10, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step10() {
    dumpDatabaseNames();
    TestRunner.completeTest();
  }
})();
