// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests IndexedDB tree element on resources panel.\n`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.loadModule('console_test_runner');

  var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
  var indexedDBModel;
  var withoutIndexedDBURL = 'http://localhost:8000/inspector/indexeddb/resources/without-indexed-db.html';
  var originalURL = 'http://127.0.0.1:8000/devtools/indexeddb/resources-panel.js';
  var databaseName = 'testDatabase';
  var objectStoreName = 'testObjectStore';
  var indexName = 'testIndexName';

  function createDatabase(callback) {
    ApplicationTestRunner.createDatabase(mainFrameId, databaseName, step2);

    function step2() {
      ApplicationTestRunner.createObjectStore(mainFrameId, databaseName, objectStoreName, '', false, step3);
    }

    function step3() {
      ApplicationTestRunner.createObjectStoreIndex(
          mainFrameId, databaseName, objectStoreName, indexName, '', false, false, callback);
    }
  }

  function deleteDatabase(callback) {
    ApplicationTestRunner.deleteObjectStoreIndex(mainFrameId, databaseName, objectStoreName, indexName, step2);

    function step2() {
      ApplicationTestRunner.deleteObjectStore(mainFrameId, databaseName, objectStoreName, step3);
    }

    function step3() {
      ApplicationTestRunner.deleteDatabase(mainFrameId, databaseName, callback);
    }
  }

  // Start with non-resources panel.
  UI.viewManager.showView('console');

  TestRunner.addSniffer(Resources.IndexedDBTreeElement.prototype, '_indexedDBAdded', indexedDBAdded, true);
  function indexedDBAdded() {
    TestRunner.addResult('Database added.');
  }

  TestRunner.addSniffer(Resources.IndexedDBTreeElement.prototype, '_indexedDBRemoved', indexedDBRemoved, true);
  function indexedDBRemoved() {
    TestRunner.addResult('Database removed.');
  }

  TestRunner.addSniffer(Resources.IndexedDBTreeElement.prototype, '_indexedDBLoaded', indexedDBLoaded, true);
  function indexedDBLoaded() {
    TestRunner.addResult('Database loaded.');
  }

  // Switch to resources panel.
  UI.viewManager.showView('resources');

  TestRunner.addResult('Expanded IndexedDB tree element.');
  UI.panels.resources._sidebar.indexedDBListTreeElement.expand();
  ApplicationTestRunner.dumpIndexedDBTree();
  TestRunner.addResult('Created database.');
  createDatabase(databaseCreated);

  function databaseCreated() {
    indexedDBModel = ApplicationTestRunner.indexedDBModel();
    indexedDBModel.addEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, databaseLoaded);
    UI.panels.resources._sidebar.indexedDBListTreeElement.refreshIndexedDB();
  }

  function databaseLoaded() {
    indexedDBModel.removeEventListener(Resources.IndexedDBModel.Events.DatabaseLoaded, databaseLoaded);
    ApplicationTestRunner.dumpIndexedDBTree();
    TestRunner.addResult('Navigated to another security origin.');
    TestRunner.navigate(withoutIndexedDBURL, navigatedAway);
  }

  function navigatedAway() {
    ApplicationTestRunner.dumpIndexedDBTree();
    TestRunner.addResult('Navigated back.');
    TestRunner.navigate(originalURL, navigatedBack);
  }

  function navigatedBack() {
    ApplicationTestRunner.dumpIndexedDBTree();
    TestRunner.addResult('Deleted database.');
    deleteDatabase(databaseDeleted);
  }

  function databaseDeleted() {
    UI.panels.resources._sidebar.indexedDBListTreeElement.refreshIndexedDB();
    TestRunner.addSniffer(
        Resources.IndexedDBModel.prototype, '_updateOriginDatabaseNames', databaseNamesLoadedAfterDeleting, false);
  }

  function databaseNamesLoadedAfterDeleting() {
    ApplicationTestRunner.dumpIndexedDBTree();
    UI.panels.resources._sidebar.indexedDBListTreeElement.collapse();
    TestRunner.completeTest();
  }
})();
