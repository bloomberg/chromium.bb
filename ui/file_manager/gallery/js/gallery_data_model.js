// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Data model for gallery.
 *
 * @param {!MetadataCache} metadataCache Metadata cache.
 * @param {!MetadataModel} metadataModel
 * @param {!EntryListWatcher=} opt_watcher Entry list watcher.
 * @constructor
 * @extends {cr.ui.ArrayDataModel}
 */
function GalleryDataModel(metadataCache, metadataModel, opt_watcher) {
  cr.ui.ArrayDataModel.call(this, []);

  /**
   * Metadata cache.
   * @private {!MetadataCache}
   * @const
   */
  this.metadataCache_ = metadataCache;

  /**
   * File system metadata.
   * @private {!MetadataModel}
   * @const
   */
  this.metadataModel_ = metadataModel;

  /**
   * Directory where the image is saved if the image is located in a read-only
   * volume.
   * @public {DirectoryEntry}
   */
  this.fallbackSaveDirectory = null;

  // Start to watch file system entries.
  var watcher = opt_watcher ? opt_watcher : new EntryListWatcher(this);
  watcher.getEntry = function(item) { return item.getEntry(); };
}

/**
 * Maximum number of full size image cache.
 * @type {number}
 * @const
 * @private
 */
GalleryDataModel.MAX_FULL_IMAGE_CACHE_ = 3;

/**
 * Maximum number of screen size image cache.
 * @type {number}
 * @const
 * @private
 */
GalleryDataModel.MAX_SCREEN_IMAGE_CACHE_ = 5;

GalleryDataModel.prototype = {
  __proto__: cr.ui.ArrayDataModel.prototype
};

/**
 * Saves new image.
 *
 * @param {!VolumeManager} volumeManager Volume manager instance.
 * @param {!Gallery.Item} item Original gallery item.
 * @param {!HTMLCanvasElement} canvas Canvas containing new image.
 * @param {boolean} overwrite Whether to overwrite the image to the item or not.
 * @return {!Promise} Promise to be fulfilled with when the operation completes.
 */
GalleryDataModel.prototype.saveItem = function(
    volumeManager, item, canvas, overwrite) {
  var oldEntry = item.getEntry();
  var oldMetadata = item.getMetadata();
  var oldMetadataItem = item.getMetadataItem();
  var oldLocationInfo = item.getLocationInfo();
  return new Promise(function(fulfill, reject) {
    item.saveToFile(
        volumeManager,
        this.fallbackSaveDirectory,
        overwrite,
        canvas,
        function(success) {
          if (!success) {
            reject('Failed to save the image.');
            return;
          }

          // Current entry is updated.
          // Dispatch an event.
          var event = new Event('content');
          event.item = item;
          event.oldEntry = oldEntry;
          event.metadata = item.getMetadata();
          this.dispatchEvent(event);

          if (!util.isSameEntry(oldEntry, item.getEntry())) {
            // New entry is added and the item now tracks it.
            // Add another item for the old entry.
            var anotherItem = new Gallery.Item(
                oldEntry,
                oldLocationInfo,
                oldMetadata,
                oldMetadataItem,
                this.metadataCache_,
                this.metadataModel_,
                item.isOriginal());
            // The item must be added behind the existing item so that it does
            // not change the index of the existing item.
            // TODO(hirono): Update the item index of the selection model
            // correctly.
            this.splice(this.indexOf(item) + 1, 0, anotherItem);
          }

          fulfill();
        }.bind(this));
  }.bind(this));
};

/**
 * Evicts image caches in the items.
 */
GalleryDataModel.prototype.evictCache = function() {
  // Sort the item by the last accessed date.
  var sorted = this.slice().sort(function(a, b) {
    return b.getLastAccessedDate() - a.getLastAccessedDate();
  });

  // Evict caches.
  var contentCacheCount = 0;
  var screenCacheCount = 0;
  for (var i = 0; i < sorted.length; i++) {
    if (sorted[i].contentImage) {
      if (++contentCacheCount > GalleryDataModel.MAX_FULL_IMAGE_CACHE_) {
        if (sorted[i].contentImage.parentNode) {
          console.error('The content image has a parent node.');
        } else {
          // Force to free the buffer of the canvas by assigning zero size.
          sorted[i].contentImage.width = 0;
          sorted[i].contentImage.height = 0;
          sorted[i].contentImage = null;
        }
      }
    }
    if (sorted[i].screenImage) {
      if (++screenCacheCount > GalleryDataModel.MAX_SCREEN_IMAGE_CACHE_) {
        if (sorted[i].screenImage.parentNode) {
          console.error('The screen image has a parent node.');
        } else {
          // Force to free the buffer of the canvas by assigning zero size.
          sorted[i].screenImage.width = 0;
          sorted[i].screenImage.height = 0;
          sorted[i].screenImage = null;
        }
      }
    }
  }
};
