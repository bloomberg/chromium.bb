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
    // TODO(yawano): Adds listener to onOpenResult when it becomes available.
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
              // TODO(yawano): Set custome icon to a result when the API becomes
              //     to support it.
              return {
                itemId: result.entry.toURL(),
                title: result.entry.name,
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
