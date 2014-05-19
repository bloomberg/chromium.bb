// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @param {Object.<string, string>} stringData String data.
 * @param {VolumeManager} volumeManager Volume manager.
 */
function BackgroundComponents(stringData, volumeManager) {
  /**
   * String data.
   * @type {Object.<string, string>}
   */
  this.stringData = stringData;

  /**
   * Volume manager.
   * @type {VolumeManager}
   */
  this.volumeManager = volumeManager;

  Object.freeze(this);
}

/**
 * Loads background component.
 * @return {Promise} Promise fulfilled with BackgroundComponents.
 */
BackgroundComponents.load = function() {
  var stringDataPromise = new Promise(function(fulfill) {
    chrome.fileBrowserPrivate.getStrings(function(stringData) {
      loadTimeData.data = stringData;
      fulfill(stringData);
    });
  });

  // VolumeManager should be obtained after stringData initialized.
  var volumeManagerPromise = stringDataPromise.then(function() {
    return new Promise(function(fulfill) {
      VolumeManager.getInstance(fulfill);
    });
  });

  return Promise.all([stringDataPromise, volumeManagerPromise]).then(
      function(args) {
        return new BackgroundComponents(args[0], args[1]);
      });
};

/**
 * Promise to be fulfilled with singleton instance of background components.
 * @type {Promise}
 */
var backgroundComponentsPromise = BackgroundComponents.load();

/**
 * Resolves file system names and obtains entries.
 * @param {Array.<FileEntry>} entries Names of isolated file system.
 * @return {Promise} Promise to be fulfilled with an entry array.
 */
function resolveEntries(entries) {
  return new Promise(function(fulfill, reject) {
    chrome.fileBrowserPrivate.resolveIsolatedEntries(entries,
                                                     function(externalEntries) {
      if (!chrome.runtime.lastError)
        fulfill(externalEntries);
      else
        reject(chrome.runtime.lastError);
    });
  });
}

/**
 * Obtains child entries.
 * @param {DirectoryEntry} entry Directory entry.
 * @return {Promise} Promise to be fulfilled with child entries.
 */
function getChildren(entry) {
  var reader = entry.createReader();
  var readEntries = function() {
    return new Promise(reader.readEntries.bind(reader)).then(function(entries) {
      if (entries.length === 0)
        return [];
      return readEntries().then(function(nextEntries) {
        return entries.concat(nextEntries);
      });
    });
  };
  return readEntries();
}

/**
 * Promise to be fulfilled with single application window.
 * @param {AppWindow}
 */
var appWindowPromise = null;

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  // Skip if files are not selected.
  if (!launchData || !launchData.items || launchData.items.length == 0)
    return;

  // Obtains entries in non-isolated file systems.
  // The entries in launchData are stored in the isolated file system.
  // We need to map the isolated entries to the normal entries to retrieve their
  // parent directory.
  var isolatedEntries = launchData.items.map(function(item) {
    return item.entry;
  });
  var selectedEntriesPromise = backgroundComponentsPromise.then(function() {
    return resolveEntries(isolatedEntries);
  });

  // If only 1 entry is selected, retrieve entries in the same directory.
  // Otherwise, just use the selectedEntries as an entry set.
  var allEntriesPromise = selectedEntriesPromise.then(function(entries) {
    if (entries.length === 1) {
      var parent = new Promise(entries[0].getParent.bind(entries[0]));
      return parent.then(getChildren).then(function(entries) {
        return entries.filter(FileType.isImageOrVideo);
      });
    } else {
      return entries;
    }
  });

  // Store the selected and all entries to the launchData.
  launchData.entriesPromise = Promise.all([selectedEntriesPromise,
                                           allEntriesPromise]).then(
      function(args) {
        return Object.freeze({
          selectedEntries: args[0],
          allEntries: args[1]
        });
      });

  // Close previous window.
  var closePromise;
  if (appWindowPromise) {
    closePromise = appWindowPromise.then(function(appWindow) {
      return new Promise(function(fulfill) {
        appWindow.close();
        appWindow.onClosed.addListener(fulfill);
      });
    });
  } else {
    closePromise = Promise.resolve();
  }
  var createdWindowPromise = closePromise.then(function() {
    return new Promise(function(fulfill) {
      chrome.app.window.create(
          'gallery.html',
          {
            id: 'gallery',
            minWidth: 160,
            minHeight: 100,
            frame: 'none'
          },
          function(appWindow) {
            appWindow.contentWindow.addEventListener(
                'load', fulfill.bind(null, appWindow));
          });
    });
  });
  appWindowPromise = Promise.all([
    createdWindowPromise,
    backgroundComponentsPromise,
  ]).then(function(args) {
    args[0].contentWindow.initialize(args[1]);
    return args[0];
  });

  // Open entries.
  Promise.all([
    appWindowPromise,
    allEntriesPromise,
    selectedEntriesPromise
  ]).then(function(args) {
    args[0].contentWindow.loadEntries(args[1], args[2]);
  }).catch(function(error) {
    console.error(error.stack || error);
  });
});
