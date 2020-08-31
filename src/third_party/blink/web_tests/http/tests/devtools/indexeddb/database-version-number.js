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
  var databaseName = 'testDatabase1';
  var securityOrigin = 'http://127.0.0.1:8000';
  var databaseId = new Resources.IndexedDBModel.DatabaseId(securityOrigin, databaseName);

  function dumpDatabase() {
    TestRunner.addResult('Dumping database:');
    var database = indexedDBModel._databases.get(databaseId);
    if (!database)
      return;
    TestRunner.addResult(database.databaseId.name);
    TestRunner.addResult('    version: ' + database.version);
    TestRunner.addResult('    objectStores:');
    var objectStoreNames = [];
    for (var objectStoreName in database.objectStores)
      objectStoreNames.push(objectStoreName);
    objectStoreNames.sort();
    for (var i = 0; i < objectStoreNames.length; ++i) {
      var objectStore = database.objectStores[objectStoreNames[i]];
      TestRunner.addResult('    ' + objectStore.name);
      TestRunner.addResult('        keyPath: ' + JSON.stringify(objectStore.keyPath));
      TestRunner.addResult('        autoIncrement: ' + objectStore.autoIncrement);
      TestRunner.addResult('        indexes: ');
      var indexNames = [];
      for (var indexName in objectStore.indexes)
        indexNames.push(indexName);
      indexNames.sort();
      for (var j = 0; j < indexNames.length; ++j) {
        var index = objectStore.indexes[indexNames[j]];
        TestRunner.addResult('        ' + index.name);
        TestRunner.addResult('            keyPath: ' + JSON.stringify(index.keyPath));
        TestRunner.addResult('            unique: ' + index.unique);
        TestRunner.addResult('            multiEntry: ' + index.multiEntry);
    }
  }
    TestRunner.addResult('');
}

  TestRunner.addSniffer(Resources.IndexedDBModel.prototype, '_updateOriginDatabaseNames', step2, false);

  function step2() {
    ApplicationTestRunner.createDatabaseWithVersion(mainFrameId, databaseName, 2147483647, step3);
  }

  function step3() {
    TestRunner.addSniffer(Resources.IndexedDBModel.prototype, '_updateOriginDatabaseNames', step4, false);
    indexedDBModel.refreshDatabaseNames();
  }

  function step4() {
    dumpDatabase();

    indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step5);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step5() {
    indexedDBModel.removeEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step5);
    dumpDatabase();

    ApplicationTestRunner.createObjectStore(
      mainFrameId, databaseName, 'testObjectStore1', 'test.key.path', true, step6);
  }

  function step6() {
    indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step7);
    indexedDBModel.refreshDatabase(databaseId);
  }

  function step7() {
    indexedDBModel.removeEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step7);
    dumpDatabase();

    ApplicationTestRunner.createObjectStore(mainFrameId, databaseName, 'testObjectStore2', null, false, step8);
  }

  function step8() {
    indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step9);
    indexedDBModel.refreshDatabase(databaseId);
  }


  function step9() {
    indexedDBModel.removeEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, step9);
    dumpDatabase();
    ApplicationTestRunner.deleteDatabase(mainFrameId, databaseName, step10);
  }

  async function step10() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
