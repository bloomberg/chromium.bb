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
 * The prefix of image files.
 *
 * @type {string}
 * @const
 */
camera.models.Gallery.IMAGE_PREFIX = 'IMG_';

/**
 * The prefix of video files.
 *
 * @type {string}
 * @const
 */
camera.models.Gallery.VIDEO_PREFIX = 'VID_';

/**
 * The prefix of thumbnail files.
 *
 * @type {string}
 * @const
 */
camera.models.Gallery.THUMBNAIL_PREFIX = 'thumb-';

/**
 * Picture types.
 * @enum {number}
 */
camera.models.Gallery.PictureType = Object.freeze({
  STILL: 0,  // Still pictures (images).
  MOTION: 1  // Motion pictures (video).
});

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
 *
 * @param {FileEntry} thumbnailEntry Thumbnail file entry.
 * @param {FileEntry} pictureEntry Picture file entry.
 * @param {camera.models.Gallery.PictureType} type Picture type.
 * @constructor
 */
camera.models.Gallery.Picture = function(thumbnailEntry, pictureEntry, type) {
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
   * @type {camera.models.Gallery.PictureType}
   * @private
   */
  this.pictureType_ = type;

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
  get pictureType() {
    return this.pictureType_;
  }
};

/**
 * Creates and returns an URL for a picture.
 * @param {function(string)} onSuccess Success callback with the url as a string.
 */
camera.models.Gallery.Picture.prototype.pictureURL = function(onSuccess) {
  camera.models.FileSystem.pictureURL(this.pictureEntry_, onSuccess);
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
 * @param {function()} onFailure Failure callback.
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
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
camera.models.Gallery.prototype.initialize_ = function(onSuccess, onFailure) {
  camera.models.FileSystem.getEntries(
      function(pictureEntries, thumbnailEntries) {
    this.loadStoredPictures_(
        pictureEntries, thumbnailEntries, onSuccess, onFailure);
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
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
camera.models.Gallery.prototype.loadStoredPictures_ = function(
    pictureEntries, thumbnailEntries, onSuccess, onFailure) {
  var queue = new camera.util.Queue();
  var entriesByName = {};
  for (var index = 0; index < thumbnailEntries.length; index++) {
    entriesByName[thumbnailEntries[index].name] = thumbnailEntries[index];
  }

  for (var index = pictureEntries.length - 1; index >= 0; index--) {
    var entry = pictureEntries[index];
    if (!entry.name) {
      console.warn('TODO(mtomasz): Temporary fix for a strange issue.');
      continue;
    }

    var type = (entry.name.indexOf(camera.models.Gallery.VIDEO_PREFIX) === 0) ?
        camera.models.Gallery.PictureType.MOTION :
        camera.models.Gallery.PictureType.STILL;
    queue.run(function(entry, callback) {
      var thumbnailName = camera.models.Gallery.getThumbnailName(entry.name);
      var thumbnailEntry = entriesByName[thumbnailName];
      // No thumbnail, most probably pictures taken by the old Camera App.
      // So, create the thumbnail now.
      if (!thumbnailEntry) {
        camera.models.FileSystem.pictureURL(entry, function(url) {
          this.createThumbnail_(url, type,
              function(thumbnailBlob) {
            camera.models.FileSystem.saveThumbnail(
                thumbnailName,
                thumbnailBlob,
                function(recreatedThumbnailEntry) {
                  this.pictures_.push(new camera.models.Gallery.Picture(
                      recreatedThumbnailEntry, entry, type));
                  callback();
                }.bind(this), function() {
                  // Ignore this error, since it is better to load something
                  // than nothing.
                  console.warn('Unabled to save the recreated thumbnail.');
                  callback();
                });
          }.bind(this), function() {
            // Ignore this error, since it is better to load something than
            // nothing.
            console.warn('Unable to recreate the thumbnail.');
            callback();
          });
        }.bind(this));
      } else {
        // The thumbnail is available.
        this.pictures_.push(new camera.models.Gallery.Picture(
            thumbnailEntry, entry, type));
        callback();
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
 *
 * @param {camera.models.Gallery.Picture} picture Picture to be deleted.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
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
 *
 * @param {camera.models.Gallery.Picture} picture Picture to be exported.
 * @param {FileEntry} fileEntry Target file entry.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback,
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
 * Creates a thumbnail from the original picture.
 *
 * @param {string} url Original picture as an URL.
 * @param {camera.models.Gallery.PictureType} type Original picture's type.
 * @param {function(Blob)} onSuccess Success callback with the thumbnail as a
 *     blob.
 * @param {function()} onFailure Failure callback.
 * @private
 */
camera.models.Gallery.prototype.createThumbnail_ = function(
    url, type, onSuccess, onFailure) {
  var name = (type == camera.models.Gallery.PictureType.MOTION) ?
      'video' : 'img';
  var original = document.createElement(name);

  var drawThumbnail = function() {
    var canvas = document.createElement('canvas');
    var context = canvas.getContext('2d');
    var thumbnailWidth = 480;  // Twice wider than in CSS for hi-dpi,
                               // like Pixel.
    var ratio = (name == 'video') ?
        original.videoHeight / original.videoWidth :
        original.height / original.width;
    var thumbnailHeight = Math.round(480 * ratio);
    canvas.width = thumbnailWidth;
    canvas.height = thumbnailHeight;
    context.drawImage(original, 0, 0, thumbnailWidth, thumbnailHeight);
    canvas.toBlob(function(blob) {
      if (blob) {
        onSuccess(blob);
      } else {
        onFailure();
      }
    }, 'image/jpeg');
  };

  if (name == 'video') {
    original.onloadeddata = drawThumbnail;
  } else {
    original.onload = drawThumbnail;
  }

  original.onerror = function() {
    onFailure();
  };

  original.src = url;
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
    var parseDate = function(filename) {
      var num = function(str) {
        return parseInt(str, 10);
      };

      var fileName = camera.models.Gallery.regulateFileName(filename);
      // Match numeric parts from filenames, e.g. IMG_'yyyyMMdd_HHmmss (n)'.jpg.
      var match = fileName.match(
          /_(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})(?: \((\d+)\))?/);
      return match ? new Date(num(match[1]), num(match[2]) - 1, num(match[3]),
          num(match[4]), num(match[5]), num(match[6]),
          match[7] ? num(match[7]) : 0) : null;
    };

    var dateA = parseDate(a.pictureEntry.name);
    if (dateA == null) {
      return -1;
    }
    var dateB = parseDate(b.pictureEntry.name);
    if (dateB == null) {
      return 1;
    }
    return dateA - dateB;
  });
};

/**
 * Generates a file name base for the picture.
 *
 * @param {camera.models.Gallery.PictureType} type Type of the picture.
 * @param {int} time Timestamp of the picture in millisecond.
 * @return {string} File name base.
 * @private
 */
camera.models.Gallery.generateFileNameBase_ = function(type, time) {
  function pad(n) {
    if (n < 10)
      n = '0' + n;
    return n;
  }

  // File name base will be formatted as IMG_yyyyMMdd_HHmmss.
  var prefix = (type == camera.models.Gallery.PictureType.MOTION) ?
      camera.models.Gallery.VIDEO_PREFIX : camera.models.Gallery.IMAGE_PREFIX;
  var now = new Date(time);
  var result = prefix + now.getFullYear() + pad(now.getMonth() + 1) +
      pad(now.getDate()) + '_' + pad(now.getHours()) + pad(now.getMinutes()) +
      pad(now.getSeconds());

  return result;
};

/**
 * Generates a file name extension for the picture.
 *
 * @param {camera.models.Gallery.PictureType} type Type of the picture.
 * @return {string} File name extension.
 * @private
 */
camera.models.Gallery.generateFileNameExt_ = function(type) {
  return (type == camera.models.Gallery.PictureType.MOTION) ?
      '.webm' : '.jpg';
};

/**
 * Regulates the file name to the desired format if it's a legacy file name.
 *
 * @param {string} filename File name to be regulated.
 * @return {string} File name in the desired format.
 */
camera.models.Gallery.regulateFileName = function(filename) {
  if (!filename.startsWith(camera.models.Gallery.VIDEO_PREFIX) &&
      !filename.startsWith(camera.models.Gallery.IMAGE_PREFIX)) {
    // Early pictures are in legacy filename format (crrev.com/c/310064).
    var match = filename.match(/(\d+).(?:\d+)/);
    if (match) {
      var type = camera.models.Gallery.PictureType.STILL;
      return camera.models.Gallery.generateFileNameBase_(type, parseInt(match[1], 10)) +
             camera.models.Gallery.generateFileNameExt_(type);
    }
  } else {
    var match = filename.match(/(\w{3}_\d{8}_\d{6})(?:_(\d+))(\..+)?$/);
    if (match && match[2]) {
      filename = match[1] + ' (' + match[2] + ')' + match[3];
    }
  }
  return filename;
};

/**
 * Generates thumbnail name according to the file name.
 *
 * @param {string} filename File name.
 * @return {string} Thumbnail name.
 */
camera.models.Gallery.getThumbnailName = function(filename) {
  var type = (filename.indexOf(camera.models.Gallery.VIDEO_PREFIX) === 0) ?
      camera.models.Gallery.PictureType.MOTION :
      camera.models.Gallery.PictureType.STILL;
  var thumbnailName = camera.models.Gallery.THUMBNAIL_PREFIX + filename;
  if (type == camera.models.Gallery.PictureType.MOTION) {
    thumbnailName = (thumbnailName.substr(0, thumbnailName.lastIndexOf('.')) ||
        thumbnailName) + '.jpg';
  }
  return thumbnailName;
};

/**
 * Adds a picture to the gallery and saves it in the internal storage.
 *
 * @param {Blob} blob Data of the picture to be added.
 * @param {camera.models.Gallery.PictureType} type Type of the picture to be
 *     added.
 * @param {function()} onFailure Failure callback.
 */
camera.models.Gallery.prototype.addPicture = function(
    blob, type, onFailure) {
  var fileNameBase = camera.models.Gallery.generateFileNameBase_(type, Date.now());

  camera.models.FileSystem.savePicture(
      fileNameBase + camera.models.Gallery.generateFileNameExt_(type),
      blob, function(pictureEntry) {
    this.createThumbnail_(
        URL.createObjectURL(blob), type, function(thumbnailBlob) {
      camera.models.FileSystem.saveThumbnail(
          camera.models.Gallery.getThumbnailName(pictureEntry.name),
          thumbnailBlob, function(thumbnailEntry) {
        var picture = new camera.models.Gallery.Picture(
            thumbnailEntry, pictureEntry, type);
        this.pictures_.push(picture);
        // Notify observers.
        for (var i = 0; i < this.observers_.length; i++) {
          this.observers_[i].onPictureAdded(picture);
        }
      }.bind(this), onFailure);
    }.bind(this), onFailure);
  }.bind(this), onFailure);
};
