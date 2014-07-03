// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Watches for changes in the tracked directory, including local metadata
 * changes.
 *
 * @param {MetadataCache} metadataCache Instance of MetadataCache.
 * @extends {cr.EventTarget}
 * @constructor
 */
function FileWatcher(metadataCache) {
  this.queue_ = new AsyncUtil.Queue();
  this.metadataCache_ = metadataCache;
  this.watchedDirectoryEntry_ = null;

  this.onDirectoryChangedBound_ = this.onDirectoryChanged_.bind(this);
  chrome.fileBrowserPrivate.onDirectoryChanged.addListener(
      this.onDirectoryChangedBound_);

  this.filesystemMetadataObserverId_ = null;
  this.thumbnailMetadataObserverId_ = null;
  this.driveMetadataObserverId_ = null;
}

/**
 * FileWatcher extends cr.EventTarget.
 */
FileWatcher.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Stops watching (must be called before page unload).
 */
FileWatcher.prototype.dispose = function() {
  chrome.fileBrowserPrivate.onDirectoryChanged.removeListener(
      this.onDirectoryChangedBound_);
  if (this.watchedDirectoryEntry_)
    this.resetWatchedEntry_(function() {}, function() {});
};

/**
 * Called when a file in the watched directory is changed.
 * @param {Event} event Change event.
 * @private
 */
FileWatcher.prototype.onDirectoryChanged_ = function(event) {
  if (this.watchedDirectoryEntry_ &&
      event.entry.toURL() === this.watchedDirectoryEntry_.toURL()) {
    var e = new Event('watcher-directory-changed');
    e.changedFiles = event.changedFiles;
    this.dispatchEvent(e);
  }
};

/**
 * Called when general metadata in the watched directory has been changed.
 *
 * @param {Array.<Entry>} entries Array of entries.
 * @param {Object.<string, Object>} properties Map from entry URLs to metadata
 *     properties.
 * @private
 */
FileWatcher.prototype.onFilesystemMetadataChanged_ = function(
    entries, properties) {
  this.dispatchMetadataEvent_('filesystem', entries, properties);
};

/**
 * Called when thumbnail metadata in the watched directory has been changed.
 *
 * @param {Array.<Entry>} entries Array of entries.
 * @param {Object.<string, Object>} properties Map from entry URLs to metadata
 *     properties.
 * @private
 */
FileWatcher.prototype.onThumbnailMetadataChanged_ = function(
    entries, properties) {
  this.dispatchMetadataEvent_('thumbnail', entries, properties);
};

/**
 * Called when drive metadata in the watched directory has been changed.
 *
 * @param {Array.<Entry>} entries Array of entries.
 * @param {Object.<string, Object>} properties Map from entry URLs to metadata
 *     properties.
 * @private
 */
FileWatcher.prototype.onDriveMetadataChanged_ = function(
    entries, properties) {
  this.dispatchMetadataEvent_('drive', entries, properties);
};

/**
 * Dispatches an event about detected change in metadata within the tracked
 * directory.
 *
 * @param {string} type Type of the metadata change.
 * @param {Array.<Entry>} entries Array of entries.
 * @param {Object.<string, Object>} properties Map from entry URLs to metadata
 *     properties.
 * @private
 */
FileWatcher.prototype.dispatchMetadataEvent_ = function(
    type, entries, properties) {
  var e = new Event('watcher-metadata-changed');
  e.metadataType = type;
  e.entries = entries;
  e.properties = properties;
  this.dispatchEvent(e);
};

/**
 * Changes the watched directory. In case of a fake entry, the watch is
 * just released, since there is no reason to track a fake directory.
 *
 * @param {!DirectoryEntry|!Object} entry Directory entry to be tracked, or the
 *     fake entry.
 * @param {function()} callback Completion callback.
 */
FileWatcher.prototype.changeWatchedDirectory = function(entry, callback) {
  if (!util.isFakeEntry(entry)) {
    this.changeWatchedEntry_(
        entry,
        callback,
        function() {
          console.error(
             'Unable to change the watched directory to: ' + entry.toURL());
          callback();
        });
  } else {
    this.resetWatchedEntry_(
        callback,
        function() {
          console.error('Unable to reset the watched directory.');
          callback();
        });
  }
};

/**
 * Resets the watched entry to the passed directory.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onError Error callback.
 * @private
 */
FileWatcher.prototype.resetWatchedEntry_ = function(onSuccess, onError) {
  // Run the tasks in the queue to avoid races.
  this.queue_.run(function(callback) {
    // Release the watched directory.
    if (this.watchedDirectoryEntry_) {
      chrome.fileBrowserPrivate.removeFileWatch(
          this.watchedDirectoryEntry_.toURL(),
          function(result) {
            this.watchedDirectoryEntry_ = null;
            if (result)
              onSuccess();
            else
              onError();
            callback();
          }.bind(this));
      this.metadataCache_.removeObserver(this.filesystemMetadataObserverId_);
      this.metadataCache_.removeObserver(this.thumbnailMetadataObserverId_);
      this.metadataCache_.removeObserver(this.driveMetadataObserverId_);
    } else {
      onSuccess();
      callback();
    }
  }.bind(this));
};

/**
 * Sets the watched entry to the passed directory.
 *
 * @param {!DirectoryEntry} entry Directory to be watched.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onError Error callback.
 * @private
 */
FileWatcher.prototype.changeWatchedEntry_ = function(
    entry, onSuccess, onError) {
  var setEntryClosure = function() {
    // Run the tasks in the queue to avoid races.
    this.queue_.run(function(callback) {
      chrome.fileBrowserPrivate.addFileWatch(
          entry.toURL(),
          function(result) {
            if (!result) {
              this.watchedDirectoryEntry_ = null;
              onError();
            } else {
              this.watchedDirectoryEntry_ = entry;
              onSuccess();
            }
            callback();
          }.bind(this));
      this.filesystemMetadataObserverId_ = this.metadataCache_.addObserver(
        entry,
        MetadataCache.CHILDREN,
        'filesystem',
        this.onFilesystemMetadataChanged_.bind(this));
      this.thumbnailMetadataObserverId_ = this.metadataCache_.addObserver(
        entry,
        MetadataCache.CHILDREN,
        'thumbnail',
        this.onThumbnailMetadataChanged_.bind(this));
      this.driveMetadataObserverId_ = this.metadataCache_.addObserver(
        entry,
        MetadataCache.CHILDREN,
        'drive',
        this.onDriveMetadataChanged_.bind(this));
    }.bind(this));
  }.bind(this);

  // Reset the watched directory first, then set the new watched directory.
  this.resetWatchedEntry_(setEntryClosure, onError);
};

/**
 * @return {DirectoryEntry} Current watched directory entry.
 */
FileWatcher.prototype.getWatchedDirectoryEntry = function() {
  return this.watchedDirectoryEntry_;
};
