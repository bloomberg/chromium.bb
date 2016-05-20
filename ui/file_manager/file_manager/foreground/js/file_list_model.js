// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * File list.
 * @param {!MetadataModel} metadataModel
 * @constructor
 * @extends {cr.ui.ArrayDataModel}
 */
function FileListModel(metadataModel) {
  cr.ui.ArrayDataModel.call(this, []);

  /**
   * @private {!MetadataModel}
   * @const
   */
  this.metadataModel_ = metadataModel;

  // Initialize compare functions.
  this.setCompareFunction('name',
      /** @type {function(*, *): number} */ (this.compareName_.bind(this)));
  this.setCompareFunction('modificationTime',
      /** @type {function(*, *): number} */ (this.compareMtime_.bind(this)));
  this.setCompareFunction('size',
      /** @type {function(*, *): number} */ (this.compareSize_.bind(this)));
  this.setCompareFunction('type',
      /** @type {function(*, *): number} */ (this.compareType_.bind(this)));

  /**
   * Whether this file list is sorted in descending order.
   * @type {boolean}
   * @private
   */
  this.isDescendingOrder_ = false;

  /**
   * The number of folders in the list.
   * @private {number}
   */
  this.numFolders_ = 0;

  /**
   * The number of files in the list.
   * @private {number}
   */
  this.numFiles_ = 0;

  /**
   * The number of image files in the list.
   * @private {number}
   */
  this.numImageFiles_ = 0;
}

/**
 * @param {!Object} fileType Type object returned by FileType.getType().
 * @return {string} Localized string representation of file type.
 */
FileListModel.getFileTypeString = function(fileType) {
  if (fileType.subtype)
    return strf(fileType.name, fileType.subtype);
  else
    return str(fileType.name);
};

FileListModel.prototype = {
  __proto__: cr.ui.ArrayDataModel.prototype
};

/**
 * Sorts data model according to given field and direction and dispathes
 * sorted event.
 * @param {string} field Sort field.
 * @param {string} direction Sort direction.
 * @override
 */
FileListModel.prototype.sort = function(field, direction) {
  this.isDescendingOrder_ = direction === 'desc';
  cr.ui.ArrayDataModel.prototype.sort.call(this, field, direction);
};

/**
 * Called before a sort happens so that you may fetch additional data
 * required for the sort.
 * @param {string} field Sort field.
 * @param {function()} callback The function to invoke when preparation
 *     is complete.
 * @override
 */
FileListModel.prototype.prepareSort = function(field, callback) {
  // Starts the actual sorting immediately as we don't need any preparation to
  // sort the file list and we want to start actual sorting as soon as possible
  // after we get the |this.isDescendingOrder_| value in sort().
  callback();
};

/**
 * Removes and adds items to the model.
 * @param {number} index The index of the item to update.
 * @param {number} deleteCount The number of items to remove.
 * @param {...*} var_args The items to add.
 * @return {!Array} An array with the removed items.
 * @override
 */
FileListModel.prototype.splice = function(index, deleteCount, var_args) {
  for (var i = index; i < index + deleteCount; i++) {
    if (i >= 0 && i < this.length)
      this.onRemoveEntryFromList_(/** @type {!Entry} */ (this.item(i)));
  }
  for (var i = 2; i < arguments.length; i++) {
    this.onAddEntryToList_(arguments[i]);
  }

  return cr.ui.ArrayDataModel.prototype.splice.apply(this, arguments);
};

/**
 * @override
 */
FileListModel.prototype.replaceItem = function(oldItem, newItem) {
  this.onRemoveEntryFromList_(oldItem);
  this.onAddEntryToList_(newItem);

  cr.ui.ArrayDataModel.prototype.replaceItem.apply(this, arguments);
};

/**
 * Returns the number of files in this file list.
 * @return {number} The number of files.
 */
FileListModel.prototype.getFileCount = function() {
  return this.numFiles_;
};

/**
 * Returns the number of folders in this file list.
 * @return {number} The number of folders.
 */
FileListModel.prototype.getFolderCount = function() {
  return this.numFolders_;
};

/**
 * Returns true if image files are dominant in this file list.
 * @return {boolean}
 */
FileListModel.prototype.isImageDominant = function() {
  return this.numFiles_ >= 0 &&
      this.numImageFiles_ / this.numFiles_ >= 0.8;
};

/**
 * Updates the statistics about contents when new entry is about to be added.
 * @param {Entry} entry Entry of the new item.
 * @private
 */
FileListModel.prototype.onAddEntryToList_ = function(entry) {
  if (entry.isDirectory)
    this.numFolders_++;
  else
    this.numFiles_++;

  var mimeType = this.metadataModel_.getCache([entry],
      ['contentMimeType'])[0].contentMimeType;
  if (FileType.isImage(entry, mimeType) || FileType.isRaw(entry, mimeType))
    this.numImageFiles_++;
};

/**
 * Updates the statistics about contents when an entry is about to be removed.
 * @param {Entry} entry Entry of the item to be removed.
 * @private
 */
FileListModel.prototype.onRemoveEntryFromList_ = function(entry) {
  if (entry.isDirectory)
    this.numFolders_--;
  else
    this.numFiles_--;

  var mimeType = this.metadataModel_.getCache([entry],
      ['contentMimeType'])[0].contentMimeType;
  if (FileType.isImage(entry, mimeType) || FileType.isRaw(entry, mimeType))
    this.numImageFiles_--;
};

/**
 * Compares entries by name.
 * @param {!Entry} a First entry.
 * @param {!Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileListModel.prototype.compareName_ = function(a, b) {
  // Directories always precede files.
  if (a.isDirectory !== b.isDirectory)
    return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;

  return util.compareName(a, b);
};

/**
 * Compares entries by mtime first, then by name.
 * @param {Entry} a First entry.
 * @param {Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileListModel.prototype.compareMtime_ = function(a, b) {
  // Directories always precede files.
  if (a.isDirectory !== b.isDirectory)
    return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;

  var properties =
      this.metadataModel_.getCache([a, b], ['modificationTime']);
  var aTime = properties[0].modificationTime || 0;
  var bTime = properties[1].modificationTime || 0;

  if (aTime > bTime)
    return 1;

  if (aTime < bTime)
    return -1;

  return util.compareName(a, b);
};

/**
 * Compares entries by size first, then by name.
 * @param {Entry} a First entry.
 * @param {Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileListModel.prototype.compareSize_ = function(a, b) {
  // Directories always precede files.
  if (a.isDirectory !== b.isDirectory)
    return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;

  var properties = this.metadataModel_.getCache([a, b], ['size']);
  var aSize = properties[0].size || 0;
  var bSize = properties[1].size || 0;

  return aSize !== bSize ? aSize - bSize : util.compareName(a, b);
};

/**
 * Compares entries by type first, then by subtype and then by name.
 * @param {Entry} a First entry.
 * @param {Entry} b Second entry.
 * @return {number} Compare result.
 * @private
 */
FileListModel.prototype.compareType_ = function(a, b) {
  // Directories always precede files.
  if (a.isDirectory !== b.isDirectory)
    return a.isDirectory === this.isDescendingOrder_ ? 1 : -1;

  var properties = this.metadataModel_.getCache([a, b], ['contentMimeType']);
  var aType = FileListModel.getFileTypeString(
      FileType.getType(a, properties[0].contentMimeType));
  var bType = FileListModel.getFileTypeString(
      FileType.getType(b, properties[1].contentMimeType));

  var result = util.collator.compare(aType, bType);
  return result !== 0 ? result : util.compareName(a, b);
};
