// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {!MockVolumeManager}
 */
var volumeManager;

/**
 * @type {!FileSystem}
 */
var driveFileSystem;

/**
 * @type {!FileSystem}
 */
var providedFileSystem;

/**
 * @type {!MockDriveSyncHandler}
 */
var driveSyncHandler;

/**
 * MockFolderShortcutsModel
 * @extends {FolderShortcutsDataModel}
 * @constructor
 */
function MockFolderShortcutsModel() {
  this.has = false;
}

MockFolderShortcutsModel.prototype.exists = function() {
  return this.has;
};

MockFolderShortcutsModel.prototype.add = function(entry) {
  this.has = true;
  return 0;
};

MockFolderShortcutsModel.prototype.remove = function(entry) {
  this.has = false;
  return 0;
};

/**
 * @type {!MockFolderShortcutsModel}
 */
var shortcutsModel;

/**
 * MockUI
 * @extends {ActionModelUI}
 * @constructor
 */
function MockUI() {
  this.listContainer = /** @type {!ListContainer} */ ({
    currentView: {
      updateListItemsMetadata: function() {},
    }
  });

  this.alertDialog = /** @type {!FilesAlertDialog} */ ({
    showHtml: function() {},
  });

  this.errorDialog = /** @type {!ErrorDialog} */ ({
    showHtml: function() {},
  });
}

/**
 * @type {!MockUI}
 */
var ui;

function setUp() {
  // Mock loadTimeData strings.
  window.loadTimeData.getString = id => id;
  window.loadTimeData.data = {};

  // Mock Chrome APIs.
  var mockChrome = {
    runtime: {
      lastError: null,
    },
    fileManagerPrivate: {
      // The following closures are set per test case.
      getCustomActions: null,
      executeCustomAction: null,
      pinDriveFile: null,
    },
  };
  installMockChrome(mockChrome);
  new MockCommandLinePrivate();

  // Setup Drive file system.
  volumeManager = new MockVolumeManager();
  var type = VolumeManagerCommon.VolumeType.DRIVE;
  driveFileSystem =
      assert(volumeManager.getCurrentProfileVolumeInfo(type).fileSystem);

  // Setup Provided file system.
  type = VolumeManagerCommon.VolumeType.PROVIDED;
  volumeManager.createVolumeInfo(type, 'provided', 'Provided');
  providedFileSystem =
      assert(volumeManager.getCurrentProfileVolumeInfo(type).fileSystem);

  // Create mock action model components.
  shortcutsModel = new MockFolderShortcutsModel();
  driveSyncHandler = new MockDriveSyncHandler();
  ui = new MockUI();
}

/**
 * Tests that the correct actions are available for a Google Drive directory.
 */
function testDriveDirectoryEntry(callback) {
  driveFileSystem.entries['/test'] =
      new MockDirectoryEntry(driveFileSystem, '/test');

  var metadataModel = new MockMetadataModel({
    canShare: true,
  });

  var model = new ActionsModel(volumeManager, metadataModel, shortcutsModel,
      driveSyncHandler, ui, [driveFileSystem.entries['/test']]);

  var invalidated = 0;
  model.addEventListener('invalidated', function() {
    invalidated++;
  });

  return reportPromise(model.initialize().then(function() {
    var actions = model.getActions();
    assertEquals(3, Object.keys(actions).length);

    // 'Share' should be disabled in offline mode.
    var shareAction = actions[ActionsModel.CommonActionId.SHARE];
    assertTrue(!!shareAction);
    volumeManager.driveConnectionState = {
      type: VolumeManagerCommon.DriveConnectionType.OFFLINE
    };
    assertFalse(shareAction.canExecute());

    // 'Manage in Drive' should be disabled in offline mode.
    var manageInDriveAction =
        actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
    assertTrue(!!manageInDriveAction);
    assertFalse(manageInDriveAction.canExecute());

    // 'Create Shortcut' should be enabled, until it's executed, then disabled.
    var createFolderShortcutAction =
        actions[ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT];
    assertTrue(!!createFolderShortcutAction);
    assertTrue(createFolderShortcutAction.canExecute());
    createFolderShortcutAction.execute();
    assertFalse(createFolderShortcutAction.canExecute());
    assertEquals(1, invalidated);

    // The model is invalidated, as list of actions have changed. Recreated
    // the model and check that the actions are updated.
    model = new ActionsModel(volumeManager, metadataModel, shortcutsModel,
        driveSyncHandler, ui, [driveFileSystem.entries['/test']]);
    model.addEventListener('invalidated', function() {
      invalidated++;
    });
    return model.initialize();
  }).then(function() {
    var actions = model.getActions();
    assertEquals(4, Object.keys(actions).length);
    assertTrue(!!actions[ActionsModel.CommonActionId.SHARE]);
    assertTrue(!!actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE]);
    assertTrue(!!actions[ActionsModel.InternalActionId.REMOVE_FOLDER_SHORTCUT]);

    // 'Create shortcut' should be disabled.
    var createFolderShortcutAction =
        actions[ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT];
    assertTrue(!!createFolderShortcutAction);
    assertFalse(createFolderShortcutAction.canExecute());
    assertEquals(1, invalidated);
  }), callback);
}

/**
 * Tests that the correct actions are available for a Google Drive file.
 */
function testDriveFileEntry(callback) {
  driveFileSystem.entries['/test.txt'] =
      new MockFileEntry(driveFileSystem, '/test.txt');

  var metadataModel = new MockMetadataModel({
    hosted: false,
    pinned: false,
  });

  var model = new ActionsModel(volumeManager, metadataModel, shortcutsModel,
      driveSyncHandler, ui, [driveFileSystem.entries['/test.txt']]);
  var invalidated = 0;

  return reportPromise(model.initialize().then(function() {
    var actions = model.getActions();
    assertEquals(3, Object.keys(actions).length);
    assertTrue(!!actions[ActionsModel.CommonActionId.SHARE]);

    // 'Save for Offline' should be enabled.
    var saveForOfflineAction =
        actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
    assertTrue(!!saveForOfflineAction);
    assertTrue(saveForOfflineAction.canExecute());

    // 'Manage in Drive' should be enabled.
    var manageInDriveAction =
        actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
    assertTrue(!!manageInDriveAction);
    assertTrue(manageInDriveAction.canExecute());

    chrome.fileManagerPrivate.pinDriveFile = function(entry, pin, callback) {
      metadataModel.properties.pinned = true;
      assertEquals(driveFileSystem.entries['/test.txt'], entry);
      assertTrue(pin);
      callback();
    };

    // For pinning, invalidating is done asynchronously, so we need to wait
    // for it with a promise.
    return new Promise(function(fulfill, reject) {
      model.addEventListener('invalidated', function() {
        invalidated++;
        fulfill();
      });
      saveForOfflineAction.execute();
    });
  }).then(function() {
    assertTrue(metadataModel.properties.pinned);
    assertEquals(1, invalidated);

    // The model is invalidated, as list of actions have changed. Recreated
    // the model and check that the actions are updated.
    model = new ActionsModel(volumeManager, metadataModel, shortcutsModel,
        driveSyncHandler, ui, [driveFileSystem.entries['/test.txt']]);
    return model.initialize();
  }).then(function() {
    var actions = model.getActions();
    assertEquals(3, Object.keys(actions).length);
    assertTrue(!!actions[ActionsModel.CommonActionId.SHARE]);

    // 'Offline not Necessary' should be enabled.
    var offlineNotNecessaryAction =
        actions[ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY];
    assertTrue(!!offlineNotNecessaryAction);
    assertTrue(offlineNotNecessaryAction.canExecute());

    // 'Manage in Drive' should be enabled.
    var manageInDriveAction =
        actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
    assertTrue(!!manageInDriveAction);
    assertTrue(manageInDriveAction.canExecute());

    chrome.fileManagerPrivate.pinDriveFile = function(entry, pin, callback) {
      metadataModel.properties.pinned = false;
      assertEquals(driveFileSystem.entries['/test.txt'], entry);
      assertFalse(pin);
      callback();
    };

    return new Promise(function(fulfill, reject) {
      model.addEventListener('invalidated', function() {
        invalidated++;
        fulfill();
      });
      offlineNotNecessaryAction.execute();
    });
  }).then(function() {
    assertFalse(metadataModel.properties.pinned);
    assertEquals(2, invalidated);
  }), callback);
}

/**
 * Tests that a Team Drive Root entry has the correct actions available.
 */
function testTeamDriveRootEntry(callback) {
  driveFileSystem.entries['/team_drives/ABC Team'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives/ABC Team');

  var metadataModel = new MockMetadataModel({
    canShare: true,
  });

  var model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, driveSyncHandler, ui,
      [driveFileSystem.entries['/team_drives/ABC Team']]);

  return reportPromise(
      model.initialize().then(function() {
        var actions = model.getActions();
        assertEquals(2, Object.keys(actions).length);

        // "share" action is disabled for Team Drive Root entries.
        var shareAction = actions[ActionsModel.CommonActionId.SHARE];
        assertTrue(!!shareAction);
        assertFalse(shareAction.canExecute());

        // "manage in drive" action is disabled for Team Drive Root entries.
        var manageAction =
            actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
        assertTrue(!!manageAction);
        assertFalse(manageAction.canExecute());
      }),
      callback);
}

/**
 * Tests that a Team Drive directory entry has the correct actions available.
 */
function testTeamDriveDirectoryEntry(callback) {
  driveFileSystem.entries['/team_drives/ABC Team/Folder 1'] =
      new MockDirectoryEntry(driveFileSystem, '/team_drives/ABC Team/Folder 1');

  var metadataModel = new MockMetadataModel({
    canShare: true,
  });

  var model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, driveSyncHandler, ui,
      [driveFileSystem.entries['/team_drives/ABC Team/Folder 1']]);

  return reportPromise(
      model.initialize().then(function() {
        var actions = model.getActions();
        assertEquals(3, Object.keys(actions).length);

        // "Share" is enabled for Team Drive directories.
        var shareAction = actions[ActionsModel.CommonActionId.SHARE];
        assertTrue(!!shareAction);
        assertTrue(shareAction.canExecute());

        // "Manage in drive" is enabled for Team Drive directories.
        var manageAction =
            actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
        assertTrue(!!manageAction);
        assertTrue(manageAction.canExecute());

        // 'Create shortcut' should be enabled.
        var createFolderShortcutAction =
            actions[ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT];
        assertTrue(!!createFolderShortcutAction);
        assertTrue(createFolderShortcutAction.canExecute());
      }),
      callback);
}

/**
 * Tests that a Team Drive file entry has the correct actions available.
 */
function testTeamDriveFileEntry(callback) {
  driveFileSystem.entries['/team_drives/ABC Team/Folder 1/test.txt'] =
      new MockFileEntry(
          driveFileSystem, '/team_drives/ABC Team/Folder 1/test.txt');

  var metadataModel = new MockMetadataModel({
    hosted: false,
    pinned: false,
  });

  var model = new ActionsModel(
      volumeManager, metadataModel, shortcutsModel, driveSyncHandler, ui,
      [driveFileSystem.entries['/team_drives/ABC Team/Folder 1/test.txt']]);

  return reportPromise(
      model.initialize().then(function() {
        var actions = model.getActions();
        assertEquals(3, Object.keys(actions).length);

        // "save for offline" action is enabled for Team Drive file entries.
        var saveForOfflineAction =
            actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
        assertTrue(!!saveForOfflineAction);
        assertTrue(saveForOfflineAction.canExecute());

        // "share" action is enabled for Team Drive file entries.
        var shareAction = actions[ActionsModel.CommonActionId.SHARE];
        assertTrue(!!shareAction);
        assertTrue(shareAction.canExecute());

        // "manage in drive" action is enabled for Team Drive file entries.
        var manageAction =
            actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE];
        assertTrue(!!manageAction);
        assertTrue(manageAction.canExecute());
      }),
      callback);
}

/**
 * Tests that if actions are provided with getCustomActions(), they appear
 * correctly for the file.
 */
function testProvidedEntry(callback) {
  providedFileSystem.entries['/test'] =
      new MockDirectoryEntry(providedFileSystem, '/test');

  chrome.fileManagerPrivate.getCustomActions = function(entries, callback) {
    assertEquals(1, entries.length);
    assertEquals(providedFileSystem.entries['/test'], entries[0]);
    callback([
      {
        id: ActionsModel.CommonActionId.SHARE,
        title: 'Share it!'
      },
      {
        id: 'some-custom-id',
        title: 'Turn into chocolate!'
      }
    ]);
  };

  var metadataModel = new MockMetadataModel(null);

  var model = new ActionsModel(volumeManager, metadataModel, shortcutsModel,
      driveSyncHandler, ui, [providedFileSystem.entries['/test']]);

  var invalidated = 0;
  model.addEventListener('invalidated', function() {
    invalidated++;
  });

  return reportPromise(model.initialize().then(function() {
    var actions = model.getActions();
    assertEquals(2, Object.keys(actions).length);

    var shareAction = actions[ActionsModel.CommonActionId.SHARE];
    assertTrue(!!shareAction);
    // Sharing on FSP is possible even if Drive is offline. Custom actions are
    // always executable, as we don't know the actions implementation.
    volumeManager.driveConnectionState = {
      type: VolumeManagerCommon.DriveConnectionType.OFFLINE
    };
    assertTrue(shareAction.canExecute());
    assertEquals('Share it!', shareAction.getTitle());

    chrome.fileManagerPrivate.executeCustomAction = function(entries, actionId,
        callback) {
      assertEquals(1, entries.length);
      assertEquals(providedFileSystem.entries['/test'], entries[0]);
      assertEquals(ActionsModel.CommonActionId.SHARE, actionId);
      callback();
    };
    shareAction.execute();
    assertEquals(1, invalidated);

    assertTrue(!!actions['some-custom-id']);
    assertTrue(actions['some-custom-id'].canExecute());
    assertEquals('Turn into chocolate!',
        actions['some-custom-id'].getTitle());

    chrome.fileManagerPrivate.executeCustomAction = function(entries, actionId,
        callback) {
      assertEquals(1, entries.length);
      assertEquals(providedFileSystem.entries['/test'], entries[0]);
      assertEquals('some-custom-id', actionId);
      callback();
    };

    actions['some-custom-id'].execute();
    assertEquals(2, invalidated);
  }), callback);
}

/**
 * Tests that no actions are available when getCustomActions() throws an error.
 */
function testProvidedEntryWithError(callback) {
  providedFileSystem.entries['/test'] =
      new MockDirectoryEntry(providedFileSystem, '/test');

  chrome.fileManagerPrivate.getCustomActions = function(entries, callback) {
    chrome.runtime.lastError = {
      message: 'Failed to fetch custom actions.'
    };
    callback(['error']);
  };

  var metadataModel = new MockMetadataModel(null);

  var model = new ActionsModel(volumeManager, metadataModel, shortcutsModel,
      driveSyncHandler, ui, [providedFileSystem.entries['/test']]);

  return reportPromise(model.initialize().then(function() {
    var actions = model.getActions();
    assertEquals(0, Object.keys(actions).length);
  }), callback);
}
