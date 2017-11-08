// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests refreshing the database information and data views.\n`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('resources');

  var databaseName = 'testDatabase';
  var objectStoreName1 = 'testObjectStore1';
  var objectStoreName2 = 'testObjectStore2';
  var indexName = 'testIndex';
  var keyPath = 'testKey';

  var indexedDBModel = TestRunner.mainTarget.model(Resources.IndexedDBModel);
  var databaseId;

  function waitRefreshDatabase() {
    var view = UI.panels.resources._sidebar.indexedDBListTreeElement._idbDatabaseTreeElements[0]._view;
    view._refreshDatabaseButtonClicked();
    return new Promise((resolve) => {
      TestRunner.addSniffer(Resources.IDBDatabaseView.prototype, '_updatedForTests', resolve, false);
    });
  }

  function waitRefreshDatabaseRightClick() {
    idbDatabaseTreeElement._refreshIndexedDB();
    return waitUpdateDataView();
  }

  function waitUpdateDataView() {
    return new Promise((resolve) => {
      TestRunner.addSniffer(Resources.IDBDataView.prototype, '_updatedDataForTests', resolve, false);
    });
  }

  function waitDatabaseLoaded(callback) {
    var event = indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, () => {
      Common.EventTarget.removeEventListeners([event]);
      callback();
    });
  }

  function waitDatabaseAdded(callback) {
    var event = indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseAdded, () => {
      Common.EventTarget.removeEventListeners([event]);
      callback();
    });
    UI.panels.resources._sidebar.indexedDBListTreeElement.refreshIndexedDB();
  }

  function dumpObjectStores() {
    TestRunner.addResult('Dumping ObjectStore data:');

    var idbDatabaseTreeElement = UI.panels.resources._sidebar.indexedDBListTreeElement._idbDatabaseTreeElements[0];
    for (var i = 0; i < idbDatabaseTreeElement.childCount(); ++i) {
      var objectStoreTreeElement = idbDatabaseTreeElement.childAt(i);
      TestRunner.addResult('    Object store: ' + objectStoreTreeElement.title);
      var entries = objectStoreTreeElement._view._entries;
      if (!entries.length) {
        TestRunner.addResult('            (no entries)');
        continue;
      }
      for (var j = 0; j < entries.length; ++j) {
        TestRunner.addResult('            Key = ' + entries[j].key._value + ', value = ' + entries[j].value);
      }
    }
  }

  // Initial tree
  ApplicationTestRunner.dumpIndexedDBTree();

  // Create database
  await ApplicationTestRunner.createDatabaseAsync(databaseName);
  await new Promise(waitDatabaseAdded);
  var idbDatabaseTreeElement = UI.panels.resources._sidebar.indexedDBListTreeElement._idbDatabaseTreeElements[0];
  databaseId = idbDatabaseTreeElement._databaseId;
  TestRunner.addResult('Created database.');
  ApplicationTestRunner.dumpIndexedDBTree();

  // Load indexedDb database view
  indexedDBModel.refreshDatabase(databaseId);  // Initial database refresh.
  await new Promise(waitDatabaseLoaded);       // Needed to initialize database view, otherwise
  idbDatabaseTreeElement.onselect(false);      // IDBDatabaseTreeElement.database would be undefined.
  var databaseView = idbDatabaseTreeElement._view;

  // Create first objectstore
  await ApplicationTestRunner.createObjectStoreAsync(databaseName, objectStoreName1, indexName, keyPath);
  await waitRefreshDatabase();
  TestRunner.addResult('Created first objectstore.');
  ApplicationTestRunner.dumpIndexedDBTree();

  // Create second objectstore
  await ApplicationTestRunner.createObjectStoreAsync(databaseName, objectStoreName2, indexName, keyPath);
  await waitRefreshDatabase();
  TestRunner.addResult('Created second objectstore.');
  ApplicationTestRunner.dumpIndexedDBTree();

  // Load objectstore data views
  for (var i = 0; i < idbDatabaseTreeElement.childCount(); ++i) {
    var objectStoreTreeElement = idbDatabaseTreeElement.childAt(i);
    objectStoreTreeElement.onselect(false);
  }

  // Add entries
  await ApplicationTestRunner.addIDBValueAsync(databaseName, objectStoreName1, 'testKey', 'testValue');
  TestRunner.addResult('Added ' + objectStoreName1 + ' entry.');
  dumpObjectStores();

  // Refresh database view
  await waitRefreshDatabase();
  await waitUpdateDataView();  // Wait for second objectstore data to load on page.
  TestRunner.addResult('Refreshed database view.');
  dumpObjectStores();

  // Add entries
  await ApplicationTestRunner.addIDBValueAsync(databaseName, objectStoreName2, 'testKey2', 'testValue2');
  TestRunner.addResult('Added ' + objectStoreName2 + ' entry.');
  dumpObjectStores();

  // Right-click refresh database view
  await waitRefreshDatabaseRightClick();
  await waitUpdateDataView();  // Wait for second objectstore data to load on page.
  TestRunner.addResult('Right-click refreshed database.');
  dumpObjectStores();

  TestRunner.completeTest();
})();
