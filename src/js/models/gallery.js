// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for models.
 */
camera.models = camera.models || {};

/**
 * Creates the Gallery view controller.
 * @constructor
 */
camera.models.Gallery = function() {
  /**
   * @type {Array.<camera.models.Gallery.Observer>}
   * @private
   */
  this.observers_ = [];

  /**
   * @type {Promise<Array.<camera.models.Gallery.Picture>>}
   * @private
   */
  this.loaded_ = null;

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Wraps an image/video and its thumbnail as a picture in the model.
 * @param {FileEntry} thumbnailEntry Thumbnail file entry.
 * @param {FileEntry} pictureEntry Picture file entry.
 * @param {boolean} isMotionPicture True if it's a motion picture (video),
 *     false it's a still picture (image).
 * @constructor
 */
camera.models.Gallery.Picture = function(
    thumbnailEntry, pictureEntry, isMotionPicture) {
  /**
   * @type {FileEntry}
   * @private
   */
  this.thumbnailEntry_ = thumbnailEntry;

  /**
   * @type {FileEntry}
   * @private
   */
  this.pictureEntry_ = pictureEntry;

  /**
   * @type {boolean}
   * @private
   */
  this.isMotionPicture_ = isMotionPicture;

  /**
   * @type {Date}
   * @private
   */
  this.timestamp_ = camera.models.Gallery.Picture.parseTimestamp_(pictureEntry);

  // End of properties. Freeze the object.
  Object.freeze(this);
};

/**
 * Gets a picture's timestamp from its name.
 * @param {FileEntry} pictureEntry Picture file entry.
 * @return {Date} Picture timestamp.
 * @private
 */
camera.models.Gallery.Picture.parseTimestamp_ = function(pictureEntry) {
  var num = function(str) {
    return parseInt(str, 10);
  };

  var name = camera.models.FileSystem.regulatePictureName(pictureEntry);
  // Match numeric parts from filenames, e.g. IMG_'yyyyMMdd_HHmmss (n)'.jpg.
  // Assume no more than one picture taken within one millisecond.
  var match = name.match(
      /_(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})(?: \((\d+)\))?/);
  return match ? new Date(num(match[1]), num(match[2]) - 1, num(match[3]),
      num(match[4]), num(match[5]), num(match[6]),
      match[7] ? num(match[7]) : 0) : new Date(0);
};

camera.models.Gallery.Picture.prototype = {
  get thumbnailURL() {
    return this.thumbnailEntry_.toURL();
  },
  get thumbnailEntry() {
    return this.thumbnailEntry_;
  },
  get pictureEntry() {
    return this.pictureEntry_;
  },
  get isMotionPicture() {
    return this.isMotionPicture_;
  },
  get timestamp() {
    return this.timestamp_;
  }
};

/**
 * Creates and returns an URL for a picture.
 * @return {!Promise<string>} Promise for the result.
 */
camera.models.Gallery.Picture.prototype.pictureURL = function() {
  return camera.models.FileSystem.pictureURL(this.pictureEntry_);
};

/**
 * Observer interface for the pictures' model changes.
 * @constructor
 */
camera.models.Gallery.Observer = function() {
};

/**
 * Notifies about a deleted picture.
 * @param {camera.models.Gallery.Picture} picture Picture deleted.
 */
camera.models.Gallery.Observer.prototype.onPictureDeleted = function(picture) {
};

/**
 * Notifies about an added picture.
 * @param {camera.models.Gallery.Picture} picture Picture added.
 */
camera.models.Gallery.Observer.prototype.onPictureAdded = function(picture) {
};

/**
 * Loads the model.
 * @param {Array.<camera.models.Gallery.Observer>} observers Observers for
 *     the pictures' model changes.
 */
camera.models.Gallery.prototype.load = function(observers) {
  this.observers_ = observers;
  this.loaded_ = camera.models.FileSystem.getEntries().then(
      ([pictureEntries, thumbnailEntriesByName]) => {
    return this.loadStoredPictures_(pictureEntries, thumbnailEntriesByName);
  });

  this.loaded_.then(pictures => {
    pictures.forEach(picture => {
      this.notifyObservers_('onPictureAdded', picture);
    });
  }).catch(error => {
    console.warn(error);
  });
};

/**
 * Loads the pictures from the storages and adds them to the pictures model.
 * @param {Array.<FileEntry>} pictureEntries Picture entries.
 * @param {Object{string, FileEntry}} thumbnailEntriesByName Thumbanil entries
 *     mapped by thumbnail names.
 * @return {!Promise<Array.<camera.models.Gallery.Picture>>} Promise for the
 *     pictures.
 * @private
 */
camera.models.Gallery.prototype.loadStoredPictures_ = function(
    pictureEntries, thumbnailEntriesByName) {
  var wrapped = pictureEntries.filter(entry => entry.name).map(entry => {
    // Create the thumbnail if it's not cached. Ignore errors since it is
    // better to load something than nothing.
    // TODO(yuli): Remove unused thumbnails.
    var thumbnailName = camera.models.FileSystem.getThumbnailName(entry);
    var thumbnailEntry = thumbnailEntriesByName[thumbnailName];
    return this.wrapPicture_(entry, thumbnailEntry).catch(() => null);
  });

  return Promise.all(wrapped).then(pictures => {
    // Sort pictures by timestamps. The most recent picture will be at the end.
    return pictures.filter(p => p).sort((a, b) => {
      if (a.timestamp == null) {
        return -1;
      }
      if (b.timestamp == null) {
        return 1;
      }
      return a.timestamp - b.timestamp;
    });
  });
};

/**
 * Gets the last picture of the loaded pictures' model.
 * @return {!Promise<camera.models.Gallery.Picture>} Promise for the result.
 */
camera.models.Gallery.prototype.lastPicture = function() {
  return this.loaded_.then(pictures => {
    return pictures[pictures.length - 1];
  });
};

/**
 * Checks and updates the last picture of the loaded pictures' model.
 * @return {!Promise<camera.models.Gallery.Picture>} Promise for the result.
 */
camera.models.Gallery.prototype.checkLastPicture = function() {
  var last = null;
  return this.lastPicture().then(picture => {
    last = picture;
    if (camera.models.FileSystem.externalFs) {
      // Check if the external picture has not been deleted since loaded.
      return camera.models.FileSystem.hasExternalPicture(
          last.pictureEntry.name);
    } else {
      // Assume the model tracks every internal picture's deletion since loaded.
      return true;
    }
  }).then(exist => {
    if (exist) {
      return last;
    }
    last.pictureEntry = null;
    this.deletePicture(last);
    return this.checkLastPicture();
  });
};

/**
 * Deletes the picture in the pictures' model.
 * @param {camera.models.Gallery.Picture} picture Picture to be deleted.
 * @param {function(*=)} onFailure Failure callback.
 */
camera.models.Gallery.prototype.deletePicture = function(picture, onFailure) {
  this.loaded_.then(pictures => {
    var onDelete = () => {
      var index = pictures.indexOf(picture);
      pictures.splice(index, 1);
      this.notifyObservers_('onPictureDeleted', picture);
      picture.thumbnailEntry.remove(function() {});
    };
    if (picture.pictureEntry) {
      picture.pictureEntry.remove(onDelete, onFailure);
    } else {
      onDelete();
    }
  });
};

/**
 * Exports the picture to the external storage.
 * @param {camera.models.Gallery.Picture} picture Picture to be exported.
 * @param {FileEntry} fileEntry Target file entry.
 * @param {function(*=)} onFailure Failure callback,
 */
camera.models.Gallery.prototype.exportPicture = function(
    picture, fileEntry, onFailure) {
  // TODO(yuli): Handle insufficient storage.
  fileEntry.getParent(function(directory) {
    picture.pictureEntry.copyTo(
        directory, fileEntry.name, function() {}, onFailure);
  }, onFailure);
};

/**
 * Wraps file entries as a picture for the pictures' model.
 * @param {FileEntry} pictureEntry Picture file entry.
 * @param {FileEntry=} thumbnailEntry Thumbnail file entry.
 * @return {!Promise<camera.models.Gallery.Picture>} Promise for the picture.
 * @private
 */
camera.models.Gallery.prototype.wrapPicture_ = function(
    pictureEntry, thumbnailEntry) {
  var isMotionPicture = camera.models.FileSystem.hasVideoPrefix(pictureEntry);
  return new Promise((resolve, reject) => {
    if (thumbnailEntry) {
      resolve(thumbnailEntry);
    } else {
      // TODO(yuli): Add the picture even if unable to save its thumbnail.
      camera.models.FileSystem.saveThumbnail(
          isMotionPicture, pictureEntry, resolve, reject);
    }
  }).then(thumbnailEntry => {
    return new camera.models.Gallery.Picture(
        thumbnailEntry, pictureEntry, isMotionPicture);
  });
};

/**
 * Notifies observers about the added or deleted picture.
 * @param {string} fn Observers' callback function name.
 * @param {camera.models.Gallery.Picture} picture Picture added or deleted.
 * @private
 */
camera.models.Gallery.prototype.notifyObservers_ = function(fn, picture) {
  for (var i = 0; i < this.observers_.length; i++) {
    this.observers_[i][fn](picture);
  }
};

/**
 * Saves a picture that will also be added to the pictures' model.
 * @param {Blob} blob Data of the picture to be added.
 * @param {boolean} isMotionPicture Picture to be added is a video.
 * @param {function(*=)} onFailure Failure callback.
 */
camera.models.Gallery.prototype.savePicture = function(
    blob, isMotionPicture, onFailure) {
  // TODO(yuli): models.Gallery listens to models.FileSystem's file-added event
  // and then add a new picture into the model.
  var saved = new Promise(resolve => {
    if (isMotionPicture) {
      resolve(blob);
    } else {
      // Ignore errors since it is better to save something than nothing.
      // TODO(yuli): Support showing images by EXIF orientation instead.
      camera.util.orientPhoto(blob, resolve, () => {
        resolve(blob);
      });
    }
  }).then(blob => {
    return new Promise((resolve, reject) => {
      camera.models.FileSystem.savePicture(
          isMotionPicture, blob, resolve, reject);
    });
  }).then(pictureEntry => {
    return this.wrapPicture_(pictureEntry);
  });
  Promise.all([this.loaded_, saved]).then(([pictures, picture]) => {
    // TODO(yuli): Avoid async saved-pictures out of order here.
    pictures.push(picture);
    this.notifyObservers_('onPictureAdded', picture);
  }).catch(onFailure);
};
