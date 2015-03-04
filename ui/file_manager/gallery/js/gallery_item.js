// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Object representing an image item (a photo).
 *
 * @param {!FileEntry} entry Image entry.
 * @param {!EntryLocation} locationInfo Entry location information.
 * @param {!MetadataItem} metadataItem
 * @param {!ThumbnailMetadataItem} thumbnailMetadataItem
 * @param {boolean} original Whether the entry is original or edited.
 * @constructor
 * @struct
 */
Gallery.Item = function(
    entry, locationInfo, metadataItem, thumbnailMetadataItem, original) {
  /**
   * @private {!FileEntry}
   */
  this.entry_ = entry;

  /**
   * @private {!EntryLocation}
   */
  this.locationInfo_ = locationInfo;

  /**
   * @private {!MetadataItem}
   */
  this.metadataItem_ = metadataItem;

  /**
   * @private {!ThumbnailMetadataItem}
   */
  this.thumbnailMetadataItem_ = metadataItem;

  // TODO(yawano): Change this.contentImage and this.screenImage to private
  // fields and provide utility methods for them (e.g. revokeFullImageCache).
  /**
   * The content cache is used for prefetching the next image when going through
   * the images sequentially. The real life photos can be large (18Mpix = 72Mb
   * pixel array) so we want only the minimum amount of caching.
   * @type {(HTMLCanvasElement|HTMLImageElement)}
   */
  this.contentImage = null;

  /**
   * We reuse previously generated screen-scale images so that going back to a
   * recently loaded image looks instant even if the image is not in the content
   * cache any more. Screen-scale images are small (~1Mpix) so we can afford to
   * cache more of them.
   * @type {HTMLCanvasElement}
   */
  this.screenImage = null;

  /**
   * Last accessed date to be used for selecting items whose cache are evicted.
   * @type {number}
   * @private
   */
  this.lastAccessed_ = Date.now();

  /**
   * @type {boolean}
   * @private
   */
  this.original_ = original;
};

/**
 * @return {!FileEntry} Image entry.
 */
Gallery.Item.prototype.getEntry = function() { return this.entry_; };

/**
 * @return {!EntryLocation} Entry location information.
 */
Gallery.Item.prototype.getLocationInfo = function() {
  return this.locationInfo_;
};

/**
 * @return {!MetadataItem} Metadata.
 */
Gallery.Item.prototype.getMetadataItem = function() {
  return this.metadataItem_;
};

/**
 * @return {!MetadataItem} Metadata.
 */
Gallery.Item.prototype.getMetadataItem = function() {
  return this.metadataItem_;
};

/**
 * @return {!ThumbnailMetadataItem} Thumbnail metadata item.
 */
Gallery.Item.prototype.getThumbnailMetadataItem = function() {
  return this.thumbnailMetadataItem_;
};

/**
 * @return {string} File name.
 */
Gallery.Item.prototype.getFileName = function() {
  return this.entry_.name;
};

/**
 * @return {boolean} True if this image has not been created in this session.
 */
Gallery.Item.prototype.isOriginal = function() { return this.original_; };

/**
 * Obtains the last accessed date.
 * @return {number} Last accessed date.
 */
Gallery.Item.prototype.getLastAccessedDate = function() {
  return this.lastAccessed_;
};

/**
 * Updates the last accessed date.
 */
Gallery.Item.prototype.touch = function() {
  this.lastAccessed_ = Date.now();
};

// TODO: Localize?
/**
 * @type {string} Suffix for a edited copy file name.
 * @const
 */
Gallery.Item.COPY_SIGNATURE = ' - Edited';

/**
 * Regular expression to match '... - Edited'.
 * @type {!RegExp}
 * @const
 */
Gallery.Item.REGEXP_COPY_0 =
    new RegExp('^(.+)' + Gallery.Item.COPY_SIGNATURE + '$');

/**
 * Regular expression to match '... - Edited (N)'.
 * @type {!RegExp}
 * @const
 */
Gallery.Item.REGEXP_COPY_N =
    new RegExp('^(.+)' + Gallery.Item.COPY_SIGNATURE + ' \\((\\d+)\\)$');

/**
 * Creates a name for an edited copy of the file.
 *
 * @param {!DirectoryEntry} dirEntry Entry.
 * @param {function(string)} callback Callback.
 * @private
 */
Gallery.Item.prototype.createCopyName_ = function(dirEntry, callback) {
  var name = this.getFileName();

  // If the item represents a file created during the current Gallery session
  // we reuse it for subsequent saves instead of creating multiple copies.
  if (!this.original_) {
    callback(name);
    return;
  }

  var ext = '';
  var index = name.lastIndexOf('.');
  if (index != -1) {
    ext = name.substr(index);
    name = name.substr(0, index);
  }

  if (!ext.match(/jpe?g/i)) {
    // Chrome can natively encode only two formats: JPEG and PNG.
    // All non-JPEG images are saved in PNG, hence forcing the file extension.
    ext = '.png';
  }

  function tryNext(tries) {
    // All the names are used. Let's overwrite the last one.
    if (tries == 0) {
      setTimeout(callback, 0, name + ext);
      return;
    }

    // If the file name contains the copy signature add/advance the sequential
    // number.
    var matchN = Gallery.Item.REGEXP_COPY_N.exec(name);
    var match0 = Gallery.Item.REGEXP_COPY_0.exec(name);
    if (matchN && matchN[1] && matchN[2]) {
      var copyNumber = parseInt(matchN[2], 10) + 1;
      name = matchN[1] + Gallery.Item.COPY_SIGNATURE + ' (' + copyNumber + ')';
    } else if (match0 && match0[1]) {
      name = match0[1] + Gallery.Item.COPY_SIGNATURE + ' (1)';
    } else {
      name += Gallery.Item.COPY_SIGNATURE;
    }

    dirEntry.getFile(name + ext, {create: false, exclusive: false},
        tryNext.bind(null, tries - 1),
        callback.bind(null, name + ext));
  }

  tryNext(10);
};

/**
 * Writes the new item content to either the existing or a new file.
 *
 * @param {!VolumeManager} volumeManager Volume manager instance.
 * @param {!MetadataModel} metadataModel
 * @param {DirectoryEntry} fallbackDir Fallback directory in case the current
 *     directory is read only.
 * @param {boolean} overwrite Whether to overwrite the image to the item or not.
 * @param {!HTMLCanvasElement} canvas Source canvas.
 * @param {function(boolean)} callback Callback accepting true for success.
 */
Gallery.Item.prototype.saveToFile = function(
    volumeManager, metadataModel, fallbackDir, overwrite, canvas, callback) {
  ImageUtil.metrics.startInterval(ImageUtil.getMetricName('SaveTime'));

  var name = this.getFileName();

  var onSuccess = function(entry) {
    var locationInfo = volumeManager.getLocationInfo(entry);
    if (!locationInfo) {
      // Reuse old location info if it fails to obtain location info.
      locationInfo = this.locationInfo_;
    }
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('SaveResult'), 1, 2);
    ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('SaveTime'));

    this.entry_ = entry;
    this.locationInfo_ = locationInfo;

    // Updates the metadata.
    metadataModel.notifyEntriesChanged([this.entry_]);
    Promise.all([
      metadataModel.get([entry], Gallery.PREFETCH_PROPERTY_NAMES),
      new ThumbnailModel(metadataModel).get([entry])
    ]).then(function(metadataLists) {
      this.metadataItem_ = metadataLists[0][0];
      this.thumbnailMetadataItem_ = metadataLists[1][0];
      callback(true);
    }.bind(this), function() {
      callback(false);
    });
  }.bind(this);

  var onError = function(error) {
    console.error('Error saving from gallery', name, error);
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('SaveResult'), 0, 2);
    if (callback)
      callback(false);
  };

  var doSave = function(newFile, fileEntry) {
    var metadataPromise = metadataModel.get(
        [fileEntry],
        ['mediaMimeType', 'contentMimeType', 'ifd', 'exifLittleEndian']);
    metadataPromise.then(function(metadataItems) {
      fileEntry.createWriter(function(fileWriter) {
        var writeContent = function() {
          fileWriter.onwriteend = onSuccess.bind(null, fileEntry);
          var metadataItem = metadataItems[0];
          metadataItem.modificationTime = new Date();
          var metadataEncoder = ImageEncoder.encodeMetadata(
              metadataItem, canvas, /* quality for thumbnail*/ 0.8);
          // Contrary to what one might think 1.0 is not a good default. Opening
          // and saving an typical photo taken with consumer camera increases
          // its file size by 50-100%. Experiments show that 0.9 is much better.
          // It shrinks some photos a bit, keeps others about the same size, but
          // does not visibly lower the quality.
          fileWriter.write(ImageEncoder.getBlob(canvas, metadataEncoder, 0.9));
        }.bind(this);
        fileWriter.onerror = function(error) {
          onError(error);
          // Disable all callbacks on the first error.
          fileWriter.onerror = null;
          fileWriter.onwriteend = null;
        };
        if (newFile) {
          writeContent();
        } else {
          fileWriter.onwriteend = writeContent;
          fileWriter.truncate(0);
        }
      }.bind(this), onError);
    }.bind(this));
  }.bind(this);

  var getFile = function(dir, newFile) {
    dir.getFile(name, {create: newFile, exclusive: newFile},
        function(fileEntry) {
          doSave(newFile, fileEntry);
        }.bind(this), onError);
  }.bind(this);

  var checkExistence = function(dir) {
    dir.getFile(name, {create: false, exclusive: false},
        getFile.bind(null, dir, false /* existing file */),
        getFile.bind(null, dir, true /* create new file */));
  };

  var saveToDir = function(dir) {
    if (overwrite && !this.locationInfo_.isReadOnly) {
      checkExistence(dir);
    } else {
      this.createCopyName_(dir, function(copyName) {
        this.original_ = false;
        name = copyName;
        checkExistence(dir);
      }.bind(this));
    }
  }.bind(this);

  if (this.locationInfo_.isReadOnly) {
    saveToDir(fallbackDir);
  } else {
    this.entry_.getParent(saveToDir, onError);
  }
};

/**
 * Renames the item.
 *
 * @param {string} displayName New display name (without the extension).
 * @return {!Promise} Promise fulfilled with when renaming completes, or
 *     rejected with the error message.
 */
Gallery.Item.prototype.rename = function(displayName) {
  var newFileName = this.entry_.name.replace(
      ImageUtil.getDisplayNameFromName(this.entry_.name), displayName);

  if (newFileName === this.entry_.name)
    return Promise.reject('NOT_CHANGED');

  if (/^\s*$/.test(displayName))
    return Promise.reject(str('ERROR_WHITESPACE_NAME'));

  var parentDirectoryPromise = new Promise(
      this.entry_.getParent.bind(this.entry_));
  return parentDirectoryPromise.then(function(parentDirectory) {
    var nameValidatingPromise =
        util.validateFileName(parentDirectory, newFileName, true);
    return nameValidatingPromise.then(function() {
      var existingFilePromise = new Promise(parentDirectory.getFile.bind(
          parentDirectory, newFileName, {create: false, exclusive: false}));
      return existingFilePromise.then(function() {
        return Promise.reject(str('GALLERY_FILE_EXISTS'));
      }, function() {
        return new Promise(
            this.entry_.moveTo.bind(this.entry_, parentDirectory, newFileName));
      }.bind(this));
    }.bind(this));
  }.bind(this)).then(function(entry) {
    this.entry_ = entry;
  }.bind(this));
};
