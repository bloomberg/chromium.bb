// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Object representing an image item (a photo or a video).
 *
 * @param {FileEntry} entry Image entry.
 * @constructor
 */
Gallery.Item = function(entry) {
  this.entry_ = entry;
  this.original_ = true;
};

/**
 * @return {FileEntry} Image entry.
 */
Gallery.Item.prototype.getEntry = function() { return this.entry_ };

/**
 * @return {string} File name.
 */
Gallery.Item.prototype.getFileName = function() {
  return this.entry_.name;
};

/**
 * @return {boolean} True if this image has not been created in this session.
 */
Gallery.Item.prototype.isOriginal = function() { return this.original_ };

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
 * @param {Entry} dirEntry Entry.
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
    if (opt_callback) opt_callback(true);
  }.bind(this);

  function onError(error) {
    console.error('Error saving from gallery', name, error);
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('SaveResult'), 0, 2);
    if (opt_callback) opt_callback(false);
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
 * Renames the file.
 *
 * @param {string} displayName New display name (without the extension).
 * @param {function()} onSuccess Success callback.
 * @param {function()} onExists Called if the file with the new name exists.
 */
Gallery.Item.prototype.rename = function(displayName, onSuccess, onExists) {
  var newFileName = this.entry_.name.replace(
      ImageUtil.getDisplayNameFromName(this.entry_.name), displayName);

  if (newFileName === this.entry_.name)
    return;

  var onRenamed = function(entry) {
    this.entry_ = entry;
    onSuccess();
  }.bind(this);

  var onError = function() {
    console.error(
        'Rename error: "' + this.entry_.name + '" to "' + newFileName + '"');
  };

  var moveIfDoesNotExist = function(parentDir) {
    parentDir.getFile(
        newFileName,
        {create: false, exclusive: false},
        onExists,
        function() {
          this.entry_.moveTo(parentDir, newFileName, onRenamed, onError);
        }.bind(this));
  }.bind(this);

  this.entry_.getParent(moveIfDoesNotExist, onError);
};
