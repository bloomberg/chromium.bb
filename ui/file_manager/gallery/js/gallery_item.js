// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Object representing an image item (a photo).
 *
 * @param {FileEntry} entry Image entry.
 * @param {function():Promise} fethcedMediaProvider Function to provide the
 *     fetchedMedia metadata.
 * @constructor
 */
Gallery.Item = function(entry, metadata, metadataCache, original) {
  /**
   * @type {FileEntry}
   * @private
   */
  this.entry_ = entry;

  /**
   * @type {Object}
   * @private
   */
  this.metadata_ = Object.freeze(metadata);

  /**
   * @type {MetadataCache}
   * @private
   */
  this.metadataCache_ = metadataCache;

  /**
   * The content cache is used for prefetching the next image when going through
   * the images sequentially. The real life photos can be large (18Mpix = 72Mb
   * pixel array) so we want only the minimum amount of caching.
   * @type {Canvas}
   */
  this.screenImage = null;

  /**
   * We reuse previously generated screen-scale images so that going back to a
   * recently loaded image looks instant even if the image is not in the content
   * cache any more. Screen-scale images are small (~1Mpix) so we can afford to
   * cache more of them.
   * @type {Canvas}
   */
  this.contentImage = null;

  /**
   * Last accessed date to be used for selecting items whose cache are evicted.
   * @type {number}
   */
  this.lastAccessed_ = Date.now();

  /**
   * @type {boolean}
   * @private
   */
  this.original_ = original;

  Object.seal(this);
};

/**
 * @return {FileEntry} Image entry.
 */
Gallery.Item.prototype.getEntry = function() { return this.entry_; };

/**
 * @return {Object} Metadata.
 */
Gallery.Item.prototype.getMetadata = function() { return this.metadata_; };

/**
 * Obtains the latest media metadata.
 *
 * This is a heavy operation since it forces to load the image data to obtain
 * the metadata.
 * @return {Promise} Promise to be fulfilled with fetched metadata.
 */
Gallery.Item.prototype.getFetchedMedia = function() {
  return new Promise(function(fulfill, reject) {
    this.metadataCache_.getLatest(
        [this.entry_],
        'fetchedMedia',
        function(metadata) {
          if (metadata[0])
            fulfill(metadata[0]);
          else
            reject('Failed to load metadata.');
        });
  }.bind(this));
};

/**
 * Sets the metadata.
 * @param {Object} metadata New metadata.
 */
Gallery.Item.prototype.setMetadata = function(metadata) {
  this.metadata_ = Object.freeze(metadata);
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
 */
Gallery.Item.COPY_SIGNATURE = ' - Edited';

/**
 * Regular expression to match '... - Edited'.
 * @type {RegExp}
 */
Gallery.Item.REGEXP_COPY_0 =
    new RegExp('^(.+)' + Gallery.Item.COPY_SIGNATURE + '$');

/**
 * Regular expression to match '... - Edited (N)'.
 * @type {RegExp}
 */
Gallery.Item.REGEXP_COPY_N =
    new RegExp('^(.+)' + Gallery.Item.COPY_SIGNATURE + ' \\((\\d+)\\)$');

/**
 * Creates a name for an edited copy of the file.
 *
 * @param {DirectoryEntry} dirEntry Entry.
 * @param {function} callback Callback.
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
 * Writes the new item content to the file.
 *
 * @param {Entry} overrideDir Directory to save to. If null, save to the same
 *   directory as the original.
 * @param {boolean} overwrite True if overwrite, false if copy.
 * @param {HTMLCanvasElement} canvas Source canvas.
 * @param {ImageEncoder.MetadataEncoder} metadataEncoder MetadataEncoder.
 * @param {function(boolean)=} opt_callback Callback accepting true for success.
 */
Gallery.Item.prototype.saveToFile = function(
    overrideDir, overwrite, canvas, metadataEncoder, opt_callback) {
  ImageUtil.metrics.startInterval(ImageUtil.getMetricName('SaveTime'));

  var name = this.getFileName();

  var onSuccess = function(entry) {
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('SaveResult'), 1, 2);
    ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('SaveTime'));
    this.entry_ = entry;
    this.metadataCache_.clear([this.entry_], 'fetchedMedia');
    if (opt_callback)
      opt_callback(true);
  }.bind(this);

  function onError(error) {
    console.error('Error saving from gallery', name, error);
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('SaveResult'), 0, 2);
    if (opt_callback)
      opt_callback(false);
  }

  function doSave(newFile, fileEntry) {
    fileEntry.createWriter(function(fileWriter) {
      function writeContent() {
        fileWriter.onwriteend = onSuccess.bind(null, fileEntry);
        fileWriter.write(ImageEncoder.getBlob(canvas, metadataEncoder));
      }
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
    }, onError);
  }

  function getFile(dir, newFile) {
    dir.getFile(name, {create: newFile, exclusive: newFile},
        doSave.bind(null, newFile), onError);
  }

  function checkExistence(dir) {
    dir.getFile(name, {create: false, exclusive: false},
        getFile.bind(null, dir, false /* existing file */),
        getFile.bind(null, dir, true /* create new file */));
  }

  var saveToDir = function(dir) {
    if (overwrite) {
      checkExistence(dir);
    } else {
      this.createCopyName_(dir, function(copyName) {
        this.original_ = false;
        name = copyName;
        checkExistence(dir);
      }.bind(this));
    }
  }.bind(this);

  if (overrideDir) {
    saveToDir(overrideDir);
  } else {
    this.entry_.getParent(saveToDir, onError);
  }
};

/**
 * Renames the item.
 *
 * @param {string} displayName New display name (without the extension).
 * @return {Promise} Promise fulfilled with when renaming completes, or rejected
 *     with the error message.
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
