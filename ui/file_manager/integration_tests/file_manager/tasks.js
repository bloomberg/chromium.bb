// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Fake task.
 *
 * @param {boolean} isDefault Whether the task is default or not.
 * @param {string} taskId Task ID.
 * @param {string} title Title of the task.
 * @param {boolean=} opt_isGenericFileHandler Whether the task is a generic
 *     file handler.
 * @constructor
 */
function FakeTask(isDefault, taskId, title, opt_isGenericFileHandler) {
  this.driveApp = false;
  this.iconUrl = 'chrome://theme/IDR_DEFAULT_FAVICON';  // Dummy icon
  this.isDefault = isDefault;
  this.taskId = taskId;
  this.title = title;
  this.isGenericFileHandler = opt_isGenericFileHandler || false;
  Object.freeze(this);
}

/**
 * Fake tasks for a local volume.
 *
 * @type {Array.<FakeTask>}
 * @const
 */
var DOWNLOADS_FAKE_TASKS = [
  new FakeTask(true, 'dummytaskid|open-with', 'DummyAction1'),
  new FakeTask(false, 'dummytaskid-2|open-with', 'DummyAction2')
];

/**
 * Fake tasks for a drive volume.
 *
 * @type {Array.<FakeTask>}
 * @const
 */
var DRIVE_FAKE_TASKS = [
  new FakeTask(true, 'dummytaskid|drive|open-with', 'DummyAction1'),
  new FakeTask(false, 'dummytaskid-2|drive|open-with', 'DummyAction2')
];

/**
 * Sets up task tests.
 *
 * @param {string} rootPath Root path.
 * @param {Array.<FakeTask>} fakeTasks Fake tasks.
 */
function setupTaskTest(rootPath, fakeTasks) {
  return setupAndWaitUntilReady(null, rootPath).then(function(windowId) {
    return remoteCall.callRemoteTestUtil(
        'overrideTasks',
        windowId,
        [fakeTasks]).then(function() {
      return windowId;
    });
  });
}

/**
 * Tests executing the default task when there is only one task.
 *
 * @param {string} expectedTaskId Task ID expected to execute.
 * @param {string} windowId Window ID.
 */
function executeDefaultTask(expectedTaskId, windowId) {
  // Select file.
  var selectFilePromise =
      remoteCall.callRemoteTestUtil('selectFile', windowId, ['hello.txt']);

  // Double-click the file.
  var doubleClickPromise = selectFilePromise.then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.callRemoteTestUtil(
        'fakeMouseDoubleClick',
        windowId,
        ['#file-list li.table-row[selected] .filename-label span']);
  });

  // Wait until the task is executed.
  return doubleClickPromise.then(function(result) {
    chrome.test.assertTrue(!!result);
    return remoteCall.waitUntilTaskExecutes(windowId, expectedTaskId);
  });
}

/**
 * Tests to specify default action via the default action dialog.
 *
 * @param {string} expectedTaskId Task ID to be expected to newly specify as
 *     default.
 * @param {string} windowId Window ID.
 * @return {Promise} Promise to be fulfilled/rejected depends on the test
 *     result.
 */
function defaultActionDialog(expectedTaskId, windowId) {
  // Prepare expected labels.
  var expectedLabels = [
    'DummyAction1 (default)',
    'DummyAction2'
  ];

  // Select file.
  var selectFilePromise =
      remoteCall.callRemoteTestUtil('selectFile', windowId, ['hello.txt']);

  // Click the change default menu.
  var menuClickedPromise = selectFilePromise.
      then(function() {
        return remoteCall.waitForElement(windowId, '#tasks[multiple]');
      }).
      then(function() {
        return remoteCall.waitForElement(
            windowId, '#tasks-menu .change-default');
      }).
      then(function() {
        return remoteCall.callRemoteTestUtil(
            'fakeEvent', windowId, ['#tasks', 'select', {
              item: { action: 'ChangeDefaultAction' }
            }]);
      }).
      then(function(result) {
        chrome.test.assertTrue(result);
      });

  // Wait for the list of menu item is added as expected.
  var menuPreparedPromise = menuClickedPromise.then(function() {
    return repeatUntil(function() {
      // Obtains menu items.
      var menuItemsPromise = remoteCall.callRemoteTestUtil(
          'queryAllElements',
          windowId,
          ['#default-action-dialog #default-actions-list li']);

      // Compare the contents of items.
      return menuItemsPromise.then(function(items) {
        var actualLabels = items.map(function(item) { return item.text; });
        if (chrome.test.checkDeepEq(expectedLabels, actualLabels)) {
          return true;
        } else {
          return pending('Tasks do not match, expected: %j, actual: %j.',
                         expectedLabels,
                         actualLabels);
        }
      });
    });
  });

  // Click the non default item.
  var itemClickedPromise = menuPreparedPromise.
      then(function() {
        return remoteCall.callRemoteTestUtil(
            'fakeEvent',
            windowId,
            [
              '#default-action-dialog #default-actions-list li:nth-of-type(2)',
              'mousedown',
              {bubbles: true, button: 0}
            ]);
      }).
      then(function() {
        return remoteCall.callRemoteTestUtil(
            'fakeEvent',
            windowId,
            [
              '#default-action-dialog #default-actions-list li:nth-of-type(2)',
              'click',
              {bubbles: true}
            ]);
      }).
      then(function(result) {
        chrome.test.assertTrue(result);
      });

  // Wait for the dialog hidden, and the task is executed.
  var dialogHiddenPromise = itemClickedPromise.then(function() {
    return remoteCall.waitForElementLost(
        windowId, '#default-action-dialog', null);
  });

  // Execute the new default task.
  var taskButtonClicked = dialogHiddenPromise.
      then(function() {
        return remoteCall.callRemoteTestUtil(
            'fakeEvent', windowId, ['#tasks', 'click']);
      }).
      then(function(result) {
        chrome.test.assertTrue(result);
      });

  // Check the executed tasks.
  return dialogHiddenPromise.then(function() {
    return remoteCall.waitUntilTaskExecutes(windowId, expectedTaskId);
  });
}

testcase.executeDefaultTaskOnDrive = function() {
  testPromise(setupTaskTest(RootPath.DRIVE, DRIVE_FAKE_TASKS).then(
      executeDefaultTask.bind(null, 'dummytaskid|drive|open-with')));
};

testcase.executeDefaultTaskOnDownloads = function() {
  testPromise(setupTaskTest(RootPath.DOWNLOADS, DOWNLOADS_FAKE_TASKS).then(
      executeDefaultTask.bind(null, 'dummytaskid|open-with')));
};

testcase.defaultActionDialogOnDrive = function() {
  testPromise(setupTaskTest(RootPath.DRIVE, DRIVE_FAKE_TASKS).then(
      defaultActionDialog.bind(null, 'dummytaskid-2|drive|open-with')));
};

testcase.defaultActionDialogOnDownloads = function() {
  testPromise(setupTaskTest(RootPath.DOWNLOADS, DOWNLOADS_FAKE_TASKS).then(
      defaultActionDialog.bind(null, 'dummytaskid-2|open-with')));
};

testcase.genericTaskIsNotExecuted = function() {
  var tasks = [
    new FakeTask(false, 'dummytaskid|open-with', 'DummyAction1',
        true /* isGenericFileHandler */),
    new FakeTask(false, 'dummytaskid-2|open-with', 'DummyAction2',
        true /* isGenericFileHandler */)
  ];

  // When default task is not set, executeDefaultInternal_ in file_tasks.js
  // tries to show it in a browser tab. By checking the view-in-browser task is
  // executed, we check that default task is not set in this situation.
  //
  // See: src/ui/file_manager/file_manager/foreground/js/file_tasks.js&l=404
  testPromise(setupTaskTest(RootPath.DOWNLOADS, tasks)
    .then(function(windowId) {
      return executeDefaultTask(
          FILE_MANAGER_EXTENSIONS_ID + '|file|view-in-browser',
          windowId);
    }));
};

testcase.genericAndNonGenericTasksAreMixed = function() {
  var tasks = [
    new FakeTask(false, 'dummytaskid|open-with', 'DummyAction1',
        true /* isGenericFileHandler */),
    new FakeTask(false, 'dummytaskid-2|open-with', 'DummyAction2',
        false /* isGenericFileHandler */),
    new FakeTask(false, 'dummytaskid-3|open-with', 'DummyAction3',
        true /* isGenericFileHandler */)
  ];

  testPromise(setupTaskTest(RootPath.DOWNLOADS, tasks).then(
    executeDefaultTask.bind(null, 'dummytaskid-2|open-with')));
}
