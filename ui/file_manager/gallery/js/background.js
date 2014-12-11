// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!Object.<string, string>} stringData String data.
 * @param {!VolumeManager} volumeManager Volume manager.
 * @constructor
 * @struct
 */
function BackgroundComponents(stringData, volumeManager) {
  /**
   * String data.
   * @type {!Object.<string, string>}
   */
  this.stringData = stringData;

  /**
   * Volume manager.
   * @type {!VolumeManager}
   */
  this.volumeManager = volumeManager;
}

/**
 * Loads background component.
 * @return {!Promise} Promise fulfilled with BackgroundComponents.
 */
BackgroundComponents.load = function() {
  var stringDataPromise = new Promise(function(fulfill) {
    chrome.fileManagerPrivate.getStrings(function(stringData) {
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
 * Resolves file system names and obtains entries.
 * @param {!Array.<!FileEntry>} entries Names of isolated file system.
 * @return {!Promise} Promise to be fulfilled with an entry array.
 */
function resolveEntries(entries) {
  return new Promise(function(fulfill, reject) {
    chrome.fileManagerPrivate.resolveIsolatedEntries(
        entries, function(externalEntries) {
          if (!chrome.runtime.lastError)
            fulfill(externalEntries);
          else
            reject(chrome.runtime.lastError);
        });
  });
}

/**
 * Obtains the entry set from the entries passed from onLaunched events.
 * If an single entry is specified, the function returns all entries in the same
 * directory. Otherwise the function returns the passed entries.
 *
 * The function also filters non-image items and hidden items.
 *
 * @param {!Array.<!FileEntry>} originalEntries Entries passed from onLaunched
 *     events.
 * @return {!Promise} Promise to be fulfilled with entry array.
 */
function createEntrySet(originalEntries) {
  var entriesPromise;
  if (originalEntries.length === 1) {
    var parentPromise =
        new Promise(originalEntries[0].getParent.bind(originalEntries[0]));
    entriesPromise = parentPromise.then(function(parent) {
      var reader = parent.createReader();
      var readEntries = function() {
        return new Promise(reader.readEntries.bind(reader)).then(
            function(entries) {
              if (entries.length === 0)
                return [];
              return readEntries().then(function(nextEntries) {
                return entries.concat(nextEntries);
              });
            });
      };
      return readEntries();
    }).then(function(entries) {
      return entries.filter(function(entry) {
        return originalEntries[0].toURL() === entry.toURL() ||
            entry.name[0] !== '.';
      });
    });
  } else {
    entriesPromise = Promise.resolve(originalEntries);
  }

  return entriesPromise.then(function(entries) {
    return entries.filter(FileType.isImage).sort(function(a, b) {
      return a.name.localeCompare(b.name);
    });
  });
}

/**
 * Promise to be fulfilled with singleton instance of background components.
 * @type {Promise}
 */
var backgroundComponentsPromise = null;

/**
 * Promise to be fulfilled with single application window.
 * This can be null when the window is not opened.
 * @type {Promise}
 */
var appWindowPromise = null;

/**
 * Promise to be fulfilled with entries that are used for reopening the
 * application window.
 * @type {Promise}
 */
var reopenEntriesPromsie = null;

/**
 * Launches the application with entries.
 *
 * @param {!Promise} selectedEntriesPromise Promise to be fulfilled with the
 *     entries that are stored in the external file system (not in the isolated
 *     file system).
 * @return {!Promise} Promise to be fulfilled after the application is launched.
 */
function launch(selectedEntriesPromise) {
  // If there is the previous window, close the window.
  if (appWindowPromise) {
    reopenEntriesPromsie = selectedEntriesPromise;
    appWindowPromise.then(function(appWindow) {
      appWindow.close();
    });
    return Promise.reject('The window has already opened.');
  }
  reopenEntriesPromsie = null;

  // Create a new window.
  appWindowPromise = new Promise(function(fulfill) {
    chrome.app.window.create(
        'gallery.html',
        {
          id: 'gallery',
          innerBounds: {
            minWidth: 820,
            minHeight: 544
          },
          frame: 'none'
        },
        function(appWindow) {
          appWindow.contentWindow.addEventListener(
              'load', fulfill.bind(null, appWindow));
          appWindow.onClosed.addListener(function() {
            appWindowPromise = null;
            if (reopenEntriesPromsie)
              launch(reopenEntriesPromsie);
          });
        });
  });

  // If only 1 entry is selected, retrieve entries in the same directory.
  // Otherwise, just use the selectedEntries as an entry set.
  var allEntriesPromise = selectedEntriesPromise.then(createEntrySet);

  // Initialize the window document.
  return Promise.all([
    appWindowPromise,
    backgroundComponentsPromise
  ]).then(function(args) {
    var appWindow = /** @type {!AppWindow} */ (args[0]);
    var galleryWindow = /** @type {!GalleryWindow} */ (appWindow.contentWindow);
    galleryWindow.initialize(args[1]);
    return Promise.all([
      allEntriesPromise,
      selectedEntriesPromise
    ]).then(function(entries) {
      galleryWindow.loadEntries(entries[0], entries[1]);
    });
  });
}

// If the script is loaded from unit test, chrome.app.runtime is not defined.
// In this case, does not run the initialization code for the application.
if (chrome.app.runtime) {
  backgroundComponentsPromise = BackgroundComponents.load();
  chrome.app.runtime.onLaunched.addListener(function(launchData) {
    // Skip if files are not selected.
    if (!launchData || !launchData.items || launchData.items.length === 0)
      return;

    // Obtains entries in non-isolated file systems.
    // The entries in launchData are stored in the isolated file system.
    // We need to map the isolated entries to the normal entries to retrieve
    // their parent directory.
    var isolatedEntries = launchData.items.map(function(item) {
      return item.entry;
    });
    var selectedEntriesPromise = backgroundComponentsPromise.then(function() {
      return resolveEntries(isolatedEntries);
    });

    launch(selectedEntriesPromise).catch(function(error) {
      console.error(error.stack || error);
    });
  });
}

// If is is run in the browser test, wait for the test resources are installed
// as a component extension, and then load the test resources.
if (chrome.test) {
  /** @type {string} */
  window.testExtensionId = 'ejhcmmdhhpdhhgmifplfmjobgegbibkn';
  chrome.runtime.onMessageExternal.addListener(function(message) {
    if (message.name !== 'testResourceLoaded')
      return;
    var script = document.createElement('script');
    script.src =
        'chrome-extension://' + window.testExtensionId +
        '/common/test_loader.js';
    document.documentElement.appendChild(script);
  });
}
