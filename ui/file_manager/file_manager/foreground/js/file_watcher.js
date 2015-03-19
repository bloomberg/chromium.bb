// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Watches for changes in the tracked directory.
 *
 * @extends {cr.EventTarget}
 * @constructor
 */
function FileWatcher() {
  this.queue_ = new AsyncUtil.Queue();
  this.watchedDirectoryEntry_ = null;

  this.onDirectoryChangedBound_ = this.onDirectoryChanged_.bind(this);
  chrome.fileManagerPrivate.onDirectoryChanged.addListener(
      this.onDirectoryChangedBound_);
}

/**
 * FileWatcher extends cr.EventTarget.
 */
FileWatcher.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Stops watching (must be called before page unload).
 */
FileWatcher.prototype.dispose = function() {
  chrome.fileManagerPrivate.onDirectoryChanged.removeListener(
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
  var fireWatcherDirectoryChanged = function(changedFiles) {
    var e = new Event('watcher-directory-changed');

    if (changedFiles)
      e.changedFiles = changedFiles;

    this.dispatchEvent(e);
  }.bind(this);

  if (this.watchedDirectoryEntry_) {
    var eventURL = event.entry.toURL();
    var watchedDirURL = this.watchedDirectoryEntry_.toURL();

    if (eventURL === watchedDirURL) {
      fireWatcherDirectoryChanged(event.changedFiles);
    } else if (watchedDirURL.match(new RegExp('^' + eventURL))) {
      // When watched directory is deleted by the change in parent directory,
      // notify it as watcher directory changed.
      this.watchedDirectoryEntry_.getDirectory(
          this.watchedDirectoryEntry_.fullPath,
          {create: false},
          null,
          function() { fireWatcherDirectoryChanged(null); });
    }
  }
};

/**
 * Changes the watched directory. In case of a fake entry, the watch is
 * just released, since there is no reason to track a fake directory.
 *
 * @param {!DirectoryEntry|!FakeEntry} entry Directory entry to be tracked, or
 *     the fake entry.
 * @param {function()} callback Completion callback.
 */
FileWatcher.prototype.changeWatchedDirectory = function(entry, callback) {
  if (!util.isFakeEntry(entry)) {
    this.changeWatchedEntry_(
        /** @type {!DirectoryEntry} */ (entry),
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
      chrome.fileManagerPrivate.removeFileWatch(
          this.watchedDirectoryEntry_.toURL(),
          function(result) {
            this.watchedDirectoryEntry_ = null;
            if (result)
              onSuccess();
            else
              onError();
            callback();
          }.bind(this));
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
      chrome.fileManagerPrivate.addFileWatch(
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
