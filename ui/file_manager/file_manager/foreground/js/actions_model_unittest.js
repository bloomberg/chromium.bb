// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var volumeManager = null;
var metadataModel = null;
var shortcutsModel = null;
var driveSyncHandler = null;
var ui = null;
var driveFileSystem = null;
var providedFileSystem = null;

loadTimeData.data = {
  DRIVE_DIRECTORY_LABEL: '',
  DOWNLOADS_DIRECTORY_LABEL: ''
};

function MockMetadataModel() {
  this.properties = null;
}

MockMetadataModel.prototype.get = function() {
  return Promise.resolve([this.properties]);
};

MockMetadataModel.prototype.getCache = function() {
  return [this.properties];
};

MockMetadataModel.prototype.notifyEntriesChanged = function() {
};

function MockFolderShortcutsModel() {
  this.has = false;
}

MockFolderShortcutsModel.prototype.exists = function() {
  return this.has;
};

MockFolderShortcutsModel.prototype.add = function(entry) {
  this.has = true;
};

MockFolderShortcutsModel.prototype.remove = function(entry) {
  this.has = false;
};

MockDriveSyncHandler = function() {
  this.syncSuppressed = false;
};

MockDriveSyncHandler.prototype.isSyncSuppressed = function() {
  return this.syncSuppressed;
};

function MockUI() {
  this.listContainer = {
    currentView: {
      updateListItemsMetadata: function() {
      }
    }
  };

  this.alertDialog = {
    showHtml: function() {
    }
  };
};

function setUp() {
  window.chrome = {
    runtime: {
      lastError: null
    },
    fileManagerPrivate: {
      // The following closures are set per test case.
      getCustomActions: null,
      executeCustomAction: null,
      pinDriveFile: null
    },
  };

  volumeManager = new MockVolumeManager();
  volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED,
      'provided',
      'Provided');

  driveFileSystem = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE).fileSystem;
  providedFileSystem = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED).fileSystem;

  metadataModel = new MockMetadataModel();
  shortcutsModel = new MockFolderShortcutsModel();
  driveSyncHandler = new MockDriveSyncHandler();
  ui = new MockUI();
}

function testDriveDirectoryEntry(callback) {
  driveFileSystem.entries['/test'] =
      new MockDirectoryEntry(driveFileSystem, '/test', {});

  var model = new ActionsModel(volumeManager, metadataModel, shortcutsModel,
      driveSyncHandler, ui, [driveFileSystem.entries['/test']]);
  var invalidated = 0;
  model.addEventListener('invalidated', function() {
    invalidated++;
  });
  return reportPromise(model.initialize().then(function() {
    var actions = model.getActions();
    assertEquals(2, Object.keys(actions).length);

    var shareAction = actions[ActionsModel.CommonActionId.SHARE];
    assertTrue(!!shareAction);
    volumeManager.driveConnectionState = {
      type: VolumeManagerCommon.DriveConnectionType.OFFLINE
    };
    assertFalse(shareAction.canExecute());

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
    assertEquals(3, Object.keys(actions).length);

    var createFolderShortcutAction =
        actions[ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT];
    assertTrue(!!createFolderShortcutAction);
    assertFalse(createFolderShortcutAction.canExecute());
    assertEquals(1, invalidated);
  }), callback);
}

function testDriveFileEntry(callback) {
  driveFileSystem.entries['/test.txt'] =
      new MockFileEntry(driveFileSystem, '/test.txt', {});

  var model = new ActionsModel(volumeManager, metadataModel, shortcutsModel,
      driveSyncHandler, ui, [driveFileSystem.entries['/test.txt']]);
  var invalidated = 0;
  metadataModel.properties = {
    hosted: false,
    pinned: false
  };
  return reportPromise(model.initialize().then(function() {
    var actions = model.getActions();
    assertEquals(2, Object.keys(actions).length);
    assertTrue(!!actions[ActionsModel.CommonActionId.SHARE]);

    var saveForOfflineAction =
        actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE];
    assertTrue(!!saveForOfflineAction);
    assertTrue(saveForOfflineAction.canExecute());

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
    assertEquals(2, Object.keys(actions).length);
    assertTrue(!!actions[ActionsModel.CommonActionId.SHARE]);

    var offlineNotNecessaryAction =
        actions[ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY];
    assertTrue(!!offlineNotNecessaryAction);
    assertTrue(offlineNotNecessaryAction.canExecute());

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

function testProvidedEntry(callback) {
  providedFileSystem.entries['/test'] =
      new MockDirectoryEntry(providedFileSystem, '/test', {});

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

function testProvidedEntryWithError(callback) {
  providedFileSystem.entries['/test'] =
      new MockDirectoryEntry(providedFileSystem, '/test', {});

  chrome.fileManagerPrivate.getCustomActions = function(entries, callback) {
    chrome.runtime.lastError = {
      message: 'Failed to fetch custom actions.'
    };
    callback(null);
  };

  var model = new ActionsModel(volumeManager, metadataModel, shortcutsModel,
      driveSyncHandler, ui, [providedFileSystem.entries['/test']]);
  return reportPromise(model.initialize().then(function() {
    var actions = model.getActions();
    assertEquals(0, Object.keys(actions).length);
  }), callback);
}
