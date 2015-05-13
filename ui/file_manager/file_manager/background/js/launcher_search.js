// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Provides drive search results to chrome launcher.
 * @constructor
 * @struct
 */
function LauncherSearch() {
  // Launcher search provider is restricted to dev channel at now.
  if (!chrome.launcherSearchProvider)
    return;

  /**
   * Active query id. This value is set null when there is no active query.
   * @private {?string}
   */
  this.queryId_ = null;

  /**
   * True if this feature is enabled.
   * @private {boolean}
   */
  this.enabled_ = false;

  /**
   * @private {function(string, string, number)}
   */
  this.onQueryStartedBound_ = this.onQueryStarted_.bind(this);

  /**
   * @private {function(string)}
   */
  this.onQueryEndedBound_ = this.onQueryEnded_.bind(this);

  /**
   * @private {function(string)}
   */
  this.onOpenResultBound_ = this.onOpenResult_.bind(this);

  // This feature is disabled when drive is disabled.
  chrome.fileManagerPrivate.onPreferencesChanged.addListener(
      this.onPreferencesChanged_.bind(this));
  this.onPreferencesChanged_();
}

/**
 * Handles onPreferencesChanged event.
 */
LauncherSearch.prototype.onPreferencesChanged_ = function() {
  chrome.fileManagerPrivate.getPreferences(function(preferences) {
    this.initializeEventListeners_(preferences.driveEnabled);
  }.bind(this));
};

/**
 * Initialize event listeners of chrome.launcherSearchProvider.
 *
 * When drive is enabled, listen events of chrome.launcherSearchProvider and
 * provide seach resutls. When it is disabled, remove these event listeners and
 * stop providing search results.
 *
 * @param {boolean} isDriveEnabled True if drive is enabled.
 */
LauncherSearch.prototype.initializeEventListeners_ = function(isDriveEnabled) {
  // If this.enabled_ === isDriveEnabled, we don't need to change anything here.
  if (this.enabled_ === isDriveEnabled)
    return;

  // Remove event listeners if it's already enabled.
  if (this.enabled_) {
    chrome.launcherSearchProvider.onQueryStarted.removeListener(
        this.onQueryStartedBound_);
    chrome.launcherSearchProvider.onQueryEnded.removeListener(
        this.onQueryEndedBound_);
    chrome.launcherSearchProvider.onOpenResult.removeListener(
        this.onOpenResultBound_);
  }

  // Set queryId_ to null to prevent that on-going search returns search
  // results.
  this.queryId_ = null;

  // Add event listeners when drive is enabled.
  if (isDriveEnabled) {
    this.enabled_ = true;
    chrome.launcherSearchProvider.onQueryStarted.addListener(
        this.onQueryStartedBound_);
    chrome.launcherSearchProvider.onQueryEnded.addListener(
        this.onQueryEndedBound_);
    chrome.launcherSearchProvider.onOpenResult.addListener(
        this.onOpenResultBound_);
  } else {
    this.enabled_ = false;
  }
};

/**
 * Handles onQueryStarted event.
 * @param {string} queryId
 * @param {string} query
 * @param {number} limit
 */
LauncherSearch.prototype.onQueryStarted_ = function(queryId, query, limit) {
  this.queryId_ = queryId;

  chrome.fileManagerPrivate.searchDriveMetadata(
      {
        query: query,
        types: 'ALL',
        maxResults: limit
      }, function(results) {
        // If query is already changed, discard the results.
        if (queryId !== this.queryId_ || results.length === 0)
          return;

        chrome.launcherSearchProvider.setSearchResults(
            queryId,
            results.map(function(result) {
              // Use high-dpi icons since preferred icon size is 24px in the
              // current implementation.
              //
              // TODO(yawano): Use filetype_folder_shared.png for a shared
              //     folder.
              var iconUrl = chrome.runtime.getURL(
                  'foreground/images/filetype/2x/filetype_' +
                  FileType.getIcon(result.entry) + '.png');
              return {
                itemId: result.entry.toURL(),
                title: result.entry.name,
                iconUrl: iconUrl,
                // Relevance is set as 2 for all results as a temporary
                // implementation. 2 is the middle value.
                // TODO(yawano): Implement practical relevance calculation.
                relevance: 2
              };
            }));
      }.bind(this));
};

/**
 * Handles onQueryEnded event.
 * @param {string} queryId
 */
LauncherSearch.prototype.onQueryEnded_ = function(queryId) {
  this.queryId_ = null;
};

/**
 * Handles onOpenResult event.
 * @param {string} itemId
 */
LauncherSearch.prototype.onOpenResult_ = function(itemId) {
  util.urlToEntry(itemId).then(function(entry) {
    if (entry.isDirectory) {
      // If it's directory, open the directory with file manager.
      launchFileManager(
          { currentDirectoryURL: entry.toURL() },
          undefined, /* App ID */
          LaunchType.FOCUS_SAME_OR_CREATE);
    } else {
      // If the file is not directory, try to execute default task.
      chrome.fileManagerPrivate.getFileTasks([entry.toURL()], function(tasks) {
        // Select default task.
        var defaultTask = null;
        for (var i = 0; i < tasks.length; i++) {
          var task = tasks[i];
          if (task.isDefault) {
            defaultTask = task;
            break;
          }
        }

        // If we haven't picked a default task yet, then just pick the first one
        // which is not genric file hanlder as default task.
        // TODO(yawano) Share task execution logic with file_tasks.js.
        if (!defaultTask) {
          for (var i = 0; i < tasks.length; i++) {
            var task = tasks[i];
            if (!task.isGenericFileHandler) {
              defaultTask = task;
              break;
            }
          }
        }

        if (defaultTask) {
          // Execute default task.
          chrome.fileManagerPrivate.executeTask(
              defaultTask.taskId,
              [entry.toURL()],
              function(result) {
                if (result === 'opened' || result === 'message_sent')
                  return;
                this.openFileManagerWithSelectionURL_(entry.toURL());
              }.bind(this));
        } else {
          // If there is no default task for the url, open a file manager with
          // selecting it.
          // TODO(yawano): Add fallback to view-in-browser as file_tasks.js do.
          this.openFileManagerWithSelectionURL_(entry.toURL());
        }
      }.bind(this));
    }
  }.bind(this));
};

/**
 * Opens file manager with selecting a specified url.
 * @param {string} selectionURL A url to be selected.
 * @private
 */
LauncherSearch.prototype.openFileManagerWithSelectionURL_ = function(
    selectionURL) {
  launchFileManager(
      { selectionURL: selectionURL },
      undefined, /* App ID */
      LaunchType.FOCUS_SAME_OR_CREATE);
};
