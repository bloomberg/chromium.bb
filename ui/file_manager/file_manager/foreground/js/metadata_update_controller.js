// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller for list contents update.
 * @param {!ListContainer} listContainer
 * @param {!DirectoryModel} directoryModel
 * @param {!VolumeManagerWrapper} volumeManager
 * @param {!MetadataCache} metadataCache
 * @param {!FileWatcher} fileWatcher
 * @param {!FileOperationManager} fileOperationManager
 * @constructor
 */
function MetadataUpdateController(listContainer,
                                  directoryModel,
                                  volumeManager,
                                  metadataCache,
                                  fileWatcher,
                                  fileOperationManager) {
  /**
   * @type {!DirectoryModel}
   * @const
   * @private
   */
  this.directoryModel_ = directoryModel;

  /**
   * @type {!VolumeManagerWrapper}
   * @const
   * @private
   */
  this.volumeManager_ = volumeManager;

  /**
   * @type {!MetadataCache}
   * @const
   * @private
   */
  this.metadataCache_ = metadataCache;

  /**
   * @type {!ListContainer}
   * @const
   * @private
   */
  this.listContainer_ = listContainer;

  chrome.fileManagerPrivate.onPreferencesChanged.addListener(
      this.onPreferencesChanged_.bind(this));
  this.onPreferencesChanged_();

  fileWatcher.addEventListener(
      'watcher-metadata-changed', this.onWatcherMetadataChanged_.bind(this));

  // Update metadata to change 'Today' and 'Yesterday' dates.
  var today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(0);
  today.setMilliseconds(0);
  setTimeout(this.dailyUpdateModificationTime_.bind(this),
             today.getTime() + MetadataUpdateController.MILLISECONDS_IN_DAY_ -
             Date.now() + 1000);
}

/**
 * Number of milliseconds in a day.
 * @const {number}
 */
MetadataUpdateController.MILLISECONDS_IN_DAY_ = 24 * 60 * 60 * 1000;

/**
 * Clears metadata cache for the current directory and its decendents.
 */
MetadataUpdateController.prototype.refreshCurrentDirectoryMetadata =
    function() {
  var entries = this.directoryModel_.getFileList().slice();
  var directoryEntry = this.directoryModel_.getCurrentDirEntry();
  if (!directoryEntry)
    return;
  // We don't pass callback here. When new metadata arrives, we have an
  // observer registered to update the UI.

  // TODO(dgozman): refresh content metadata only when modificationTime
  // changed.
  var isFakeEntry = util.isFakeEntry(directoryEntry);
  var getEntries = (isFakeEntry ? [] : [directoryEntry]).concat(entries);
  if (!isFakeEntry)
    this.metadataCache_.clearRecursively(directoryEntry, '*');
  this.metadataCache_.get(getEntries, 'filesystem|external', null);

  var visibleItems = this.listContainer_.currentList.items;
  var visibleEntries = [];
  for (var i = 0; i < visibleItems.length; i++) {
    var index = this.listContainer_.currentList.getIndexOfListItem(
        visibleItems[i]);
    var entry = this.directoryModel_.getFileList().item(index);
    // The following check is a workaround for the bug in list: sometimes item
    // does not have listIndex, and therefore is not found in the list.
    if (entry)
      visibleEntries.push(entry);
  }
  // Refreshes the metadata.
  this.metadataCache_.getLatest(visibleEntries, 'thumbnail', null);
};

/**
 * Handles local metadata changes in the currect directory.
 * @param {Event} event Change event.
 * @private
 */
MetadataUpdateController.prototype.onWatcherMetadataChanged_ = function(event) {
  this.listContainer_.currentView.updateListItemsMetadata(
      event.metadataType, event.entries);
};

/**
 * @private
 */
MetadataUpdateController.prototype.dailyUpdateModificationTime_ = function() {
  var entries = this.directoryModel_.getFileList().slice();
  this.metadataCache_.get(
      entries,
      'filesystem',
      function() {
        this.listContainer_.currentView.updateListItemsMetadata(
            'filesystem', entries);
      }.bind(this));

  setTimeout(this.dailyUpdateModificationTime_.bind(this),
             MetadataUpdateController.MILLISECONDS_IN_DAY_);
};

/**
 * @private
 */
MetadataUpdateController.prototype.onPreferencesChanged_ = function() {
  chrome.fileManagerPrivate.getPreferences(function(prefs) {
    var use12hourClock = !prefs.use24hourClock;
    this.listContainer_.table.setDateTimeFormat(use12hourClock);
    this.refreshCurrentDirectoryMetadata();
  }.bind(this));
};

/**
 * Handler of file manager operations. Called when an entry has been
 * changed.
 * This updates directory model to reflect operation result immediately (not
 * waiting for directory update event). Also, preloads thumbnails for the
 * images of new entries.
 * See also fileOperationUtil.EventRouter.
 *
 * @param {Event} event An event for the entry change.
 * @private
 */
MetadataUpdateController.prototype.onEntriesChanged_ = function(event) {
  var kind = event.kind;
  var entries = event.entries;
  if (kind !== util.EntryChangedKind.CREATED)
    return;

  var preloadThumbnail = function(entry) {
    var locationInfo = this.volumeManager_.getLocationInfo(entry);
    if (!locationInfo)
      return;
    this.metadataCache_.getOne(entry, 'thumbnail|external',
        function(metadata) {
          var thumbnailLoader = new ThumbnailLoader(
              entry,
              ThumbnailLoader.LoaderType.CANVAS,
              metadata,
              undefined,  // Media type.
              locationInfo.isDriveBased ?
                  ThumbnailLoader.UseEmbedded.USE_EMBEDDED :
                  ThumbnailLoader.UseEmbedded.NO_EMBEDDED,
              10);  // Very low priority.
          thumbnailLoader.loadDetachedImage(function(success) {});
        });
  }.bind(this);

  for (var i = 0; i < entries.length; i++) {
    // Preload a thumbnail if the new copied entry an image.
    if (FileType.isImage(entries[i]))
      preloadThumbnail(entries[i]);
  }
};
