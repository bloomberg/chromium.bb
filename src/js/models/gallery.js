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
   * @type {Array.<camera.models.Gallery.Picture>}
   * @private
   */
  this.pictures_ = [];

  /**
   * @type {Array.<camera.models.Gallery.Observer>}
   * @private
   */
  this.observers_ = [];

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Instance of the singleton gallery model.
 * @type {camera.models.Gallery}
 * @private
 */
camera.models.Gallery.instance_ = null;

/**
 * Queue for the asynchronous getInstance() static method.
 * @type {camera.util.Queue}
 * @private
 */
camera.models.Gallery.queue_ = new camera.util.Queue();

/**
 * Wraps a picture file as well as the thumbnail file of a picture in the
 * gallery.
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

  // End of properties. Freeze the object.
  Object.freeze(this);
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
 * Observer interface for the gallery model changes.
 * @constructor
 */
camera.models.Gallery.Observer = function() {
};

/**
 * Notifies about a picture being deleted.
 * @param {camera.models.Gallery.Picture} picture Picture to be deleted.
 */
camera.models.Gallery.Observer.prototype.onPictureDeleting = function(picture) {
};

/**
 * Notifies about an added picture.
 * @param {camera.models.Gallery.Picture} picture Picture to be added.
 */
camera.models.Gallery.Observer.prototype.onPictureAdded = function(picture) {
};

/**
 * Returns a singleton instance to the model.
 * @param {function(camera.models.Gallery)} onSuccess Success callback.
 * @param {function(*=)} onFailure Failure callback.
 */
camera.models.Gallery.getInstance = function(onSuccess, onFailure) {
  camera.models.Gallery.queue_.run(function(callback) {
    if (camera.models.Gallery.instance_) {
      onSuccess(camera.models.Gallery.instance_);
      callback();
      return;
    }
    var model = new camera.models.Gallery();
    model.initialize_(function() {  // onSuccess.
      camera.models.Gallery.instance_ = model;
      onSuccess(model);
      callback();
    }, function() {  // onFailure.
      onFailure();
      callback();
    });
  });
};

camera.models.Gallery.prototype = {
  /**
   * @return {number} Number of pictures in the gallery.
   */
  get length() {
    return this.pictures_.length;
  },

  /**
   * @return {Array.<camera.models.Gallery.Picture>} Pictures' models.
   */
  get pictures() {
    return this.pictures_;
  },
};

/**
 * Initializes the gallery model.
 * @param {function()} onSuccess Success callback.
 * @param {function(*=)} onFailure Failure callback.
 * @private
 */
camera.models.Gallery.prototype.initialize_ = function(onSuccess, onFailure) {
  camera.models.FileSystem.getEntries(
      function(pictureEntries, thumbnailEntriesByName) {
    this.loadStoredPictures_(
        pictureEntries, thumbnailEntriesByName, onSuccess, onFailure);
  }.bind(this), onFailure);
};

/**
 * Adds an observer.
 * @param {camera.models.Gallery.Observer} observer Observer object.
 */
camera.models.Gallery.prototype.addObserver = function(observer) {
  this.observers_.push(observer);
};

/**
 * Removes an observer.
 * @param {camera.models.Gallery.Observer} observer Observer object.
 */
camera.models.Gallery.prototype.removeObserver = function(observer) {
  var index = this.observers_.indexOf(observer);
  this.observers_.splice(index, 1);
};

/**
 * Loads the pictures from the storages and adds them to the picture list.
 * @param {Array.<FileEntry>} pictureEntries Picture entries.
 * @param {Object{string, FileEntry}} thumbnailEntriesByName Thumbanil entries
 *     mapped by thumbnail names.
 * @param {function()} onSuccess Success callback.
 * @param {function(*=)} onFailure Failure callback.
 * @private
 */
camera.models.Gallery.prototype.loadStoredPictures_ = function(
    pictureEntries, thumbnailEntriesByName, onSuccess, onFailure) {
  var queue = new camera.util.Queue();

  for (var index = pictureEntries.length - 1; index >= 0; index--) {
    var entry = pictureEntries[index];
    if (!entry.name) {
      console.warn('TODO(mtomasz): Temporary fix for a strange issue.');
      continue;
    }

    var isMotionPicture = camera.models.FileSystem.hasVideoPrefix(entry);
    queue.run(function(entry, callback) {
      // TODO(yuli): Remove unused thumbnails.
      var thumbnailName = camera.models.FileSystem.getThumbnailName(entry);
      var thumbnailEntry = thumbnailEntriesByName[thumbnailName];
      if (thumbnailEntry) {
        this.pictures_.push(new camera.models.Gallery.Picture(
            thumbnailEntry, entry, isMotionPicture));
        callback();
      } else {
        // No thumbnail, most probably pictures taken by the old Camera App.
        // Create the thumbnail now and ignore errors since it is better to
        // load something than nothing.
        // TODO(yuli): Add the picture even if unable to save its thumbnail.
        camera.models.FileSystem.saveThumbnail(entry, thumbnailEntry => {
          this.pictures_.push(new camera.models.Gallery.Picture(
              thumbnailEntry, entry, isMotionPicture));
          callback();
        }, callback);
      }
    }.bind(this, entry));
  }

  queue.run(function(callback) {
    this.sortPictures_();
    onSuccess();
    callback();
  }.bind(this));
};

/**
 * Deletes the picture with the specified index by removing it from DOM and
 * removing from the storage.
 * @param {camera.models.Gallery.Picture} picture Picture to be deleted.
 * @param {function()} onSuccess Success callback.
 * @param {function(*=)} onFailure Failure callback.
 */
camera.models.Gallery.prototype.deletePicture = function(
    picture, onSuccess, onFailure) {
  picture.pictureEntry.remove(function() {
    picture.thumbnailEntry.remove(function() {
      // Notify observers.
      for (var i = 0; i < this.observers_.length; i++) {
        this.observers_[i].onPictureDeleting(picture);
      }
      var index = this.pictures_.indexOf(picture);
      this.pictures_.splice(index, 1);

      onSuccess();
    }.bind(this), onFailure);
  }.bind(this), onFailure);
};

/**
 * Saves the picture to the external storage.
 * @param {camera.models.Gallery.Picture} picture Picture to be exported.
 * @param {FileEntry} fileEntry Target file entry.
 * @param {function()} onSuccess Success callback.
 * @param {function(*=)} onFailure Failure callback,
 */
camera.models.Gallery.prototype.exportPicture = function(
    picture, fileEntry, onSuccess, onFailure) {
  // TODO(yuli): Handle insufficient storage.
  fileEntry.getParent(function(directory) {
    picture.pictureEntry.copyTo(
        directory, fileEntry.name, onSuccess, onFailure);
  }, onFailure);
};

/**
 * Sorts pictures by the taken order; the most recent picture will be at the end
 * of the picture array.
 * @private
 */
camera.models.Gallery.prototype.sortPictures_ = function() {
  // Sort pictures in ascending order of the taken time deduced from filenames.
  // Assume no more than one picture taken within one millisecond.
  this.pictures_.sort(function(a, b) {
    var parseDate = function(entry) {
      var num = function(str) {
        return parseInt(str, 10);
      };

      var name = camera.models.FileSystem.regulatePictureName(entry);
      // Match numeric parts from filenames, e.g. IMG_'yyyyMMdd_HHmmss (n)'.jpg.
      var match = name.match(
          /_(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})(?: \((\d+)\))?/);
      return match ? new Date(num(match[1]), num(match[2]) - 1, num(match[3]),
          num(match[4]), num(match[5]), num(match[6]),
          match[7] ? num(match[7]) : 0) : null;
    };

    var dateA = parseDate(a.pictureEntry);
    if (dateA == null) {
      return -1;
    }
    var dateB = parseDate(b.pictureEntry);
    if (dateB == null) {
      return 1;
    }
    return dateA - dateB;
  });
};

/**
 * Saves and adds a picture to the gallery model.
 * @param {Blob} blob Data of the picture to be added.
 * @param {boolean} isMotionPicture Picture to be added is a video.
 * @param {function(*=)} onFailure Failure callback.
 */
camera.models.Gallery.prototype.addPicture = function(
    blob, isMotionPicture, onFailure) {
  // TODO(yuli): models.Gallery listens to models.FileSystem's file-added event
  // and then add a new picture into the model.
  new Promise(resolve => {
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
    // TODO(yuli): Add the picture even if unable to save its thumbnail.
    camera.models.FileSystem.saveThumbnail(pictureEntry, thumbnailEntry => {
      var picture = new camera.models.Gallery.Picture(
          thumbnailEntry, pictureEntry, isMotionPicture);
      this.pictures_.push(picture);
      // Notify observers.
      for (var i = 0; i < this.observers_.length; i++) {
        this.observers_[i].onPictureAdded(picture);
      }
    }, onFailure);
  }).catch(onFailure);
};
