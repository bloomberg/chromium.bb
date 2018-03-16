// Copyright 2018 The Chromium OS Authors. All rights reserved.
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
 * Creates the file system controller.
 * @constructor
 */
camera.models.FileSystem = function() {
  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Internal file system.
 * @type {FileSystem}
 */
camera.models.FileSystem.internalFs = null;

/**
 * External file system.
 * @type {FileSystem}
 */
camera.models.FileSystem.externalFs = null;

/**
 * @type {boolean}
 */
camera.models.FileSystem.ackMigratePictures = false;

/**
 * @type {boolean}
 */
camera.models.FileSystem.doneMigratePictures = false;

/**
 * Initializes the file system model. This function should be called only once
 * to initialize the file system in the beginning.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onFailure Failure callback.
 */
camera.models.FileSystem.initialize = function(onSuccess, onFailure) {
  var initExternalFs = function(resolve) {
    if (camera.util.isChromeOS()) {
      chrome.fileSystem.getVolumeList(function(volumes) {
        if (volumes) {
          for (var i = 0; i < volumes.length; i++) {
            if (volumes[i].volumeId.indexOf('downloads:Downloads') !== -1) {
              chrome.fileSystem.requestFileSystem(
                  volumes[i], function(externalFileSystem) {
                camera.models.FileSystem.externalFs = externalFileSystem;
                resolve();
              });
              return;
            }
          }
        }
        resolve();
      });
    } else {
      resolve();
    }
  };

  var initInternalFs = function(resolve, reject) {
    webkitRequestFileSystem(window.PERSISTENT,
        768 * 1024 * 1024 /* 768MB */,
        function(inFileSystem) {
          camera.models.FileSystem.internalFs = inFileSystem;
          resolve();
        }, reject);
  };

  Promise.all([
      new Promise(initInternalFs),
      new Promise(initExternalFs),
      new Promise(camera.models.FileSystem.getLocalStorage_)]).
    then(onSuccess).
    catch(function(error) {
      console.error(error);
      onFailure('Failed to initialize file systems.');
    });
};

/**
 * Reloads the file entries from internal and external file system.
 * @param {function(Array.<FileSystemFileEntry>, Array.<FileSystemFileEntry>)}
 *     onSuccess Success callback.
 * @param {function(string)} onFailure Failure callback.
 * @private
 */
camera.models.FileSystem.reloadEntries_ = function(
    onSuccess, onFailure) {
  var readEntries = function(fs, resolve, reject) {
    if (fs == null) {
      resolve([]);
      return;
    }
    camera.models.FileSystem.readDirEntries_(fs.root, function(entries) {
      resolve(entries);
    }, function(error) {
      reject();
    });
  };

  Promise.all([
      new Promise(readEntries.bind(this, camera.models.FileSystem.internalFs)),
      new Promise(readEntries.bind(this, camera.models.FileSystem.externalFs))]).
    then(function(values) {
      onSuccess(values[0], values[1]);
    }).
    catch(function(error) {
      console.error(error);
      onFailure('Failed to read file directories.');
    });
};

/**
 * Reads file entries from a directory.
 * @param {FileSystemDirectoryEntry} dir The directory entry.
 * @param {function(Array.<FileSystemFileEntry>)} onSuccess Pass file entries.
 * @param {function(FileError)} onFailure Failure callback.
 * @private
 */
camera.models.FileSystem.readDirEntries_ = function(dir, onSuccess, onFailure) {
  var dirReader = dir.createReader();
  var entries = [];
  var readEntries = function() {
    dirReader.readEntries(function(inEntries) {
      if (inEntries.length == 0) {
        onSuccess(entries);
        return;
      }
      entries = entries.concat(inEntries);
      readEntries();
    }, onFailure);
  };

  readEntries();
};

/**
 * Checks if there is an image or video in the file entries.
 * @param {function()} onYes Yes callback.
 * @param {function()} onNo No callback.
 * @param {function(string)} onFailure Failure callback.
 * @private
 */
camera.models.FileSystem.hasInternalPictures_ = function(onYes, onNo, onFailure) {
  camera.models.FileSystem.readDirEntries_(
      camera.models.FileSystem.internalFs.root, function(entries) {
    for (var index = entries.length - 1; index >= 0; index--) {
      // Check if there is any picture other than thumbnail in file entries.
      // Pictures taken by old Camera App may not have IMG_ or VID_ prefix.
      if (!camera.models.FileSystem.isThumbnail_(entries[index])) {
        onYes();
        return;
      }
    }
    onNo();
  }, function(error) {
    onFailure('Failed to read file entries.');
  });
};

/**
 * Sets migration acknowledgement to true.
 * @privvate
 */
camera.models.FileSystem.ackMigrate_ = function() {
  camera.models.FileSystem.ackMigratePictures = true;
  chrome.storage.local.set({ackMigratePictures: 1});
};

/**
 * Sets migration status is done.
 * @private
 */
camera.models.FileSystem.doneMigrate_ = function() {
  camera.models.FileSystem.doneMigratePictures = true;
  chrome.storage.local.set({doneMigratePictures: 1});
};

/**
 * Gets migration variables from local storage.
 * @param {function()} onSuccess Success callback.
 * @private
 */
camera.models.FileSystem.getLocalStorage_ = function(onSuccess) {
  chrome.storage.local.get(
      {
        ackMigratePictures: 0, // 1: User acknowledge to migrate pictures to Downloads.
        doneMigratePictures: 0, // 1: All pictures have been migrated to Downloads.
      }, function(values) {
        camera.models.FileSystem.ackMigratePictures =
            values.ackMigratePictures >= 1;
        camera.models.FileSystem.doneMigratePictures =
            values.doneMigratePictures >= 1;
        onSuccess();
      });
};

/**
 * Checks migration is needed or not.
 * @param {function()} onYes Callback function to do migration.
 * @param {function()} onNo Callback function without migration.
 * @param {function(string)} onFailure Failure callback.
 */
camera.models.FileSystem.needMigration = function(onYes, onNo, onFailure) {
  if (camera.models.FileSystem.doneMigratePictures ||
      !camera.models.FileSystem.externalFs) {
    onNo();
  } else {
    camera.models.FileSystem.hasInternalPictures_(onYes, function() {
      // If the external FS is supported and there is no picture in
      // the internal storage, it implies users acknowledge to use
      // the external FS from now without showing a prompt dialog.
      camera.models.FileSystem.ackMigrate_();
      camera.models.FileSystem.doneMigrate_();
      onNo();
    }, onFailure);
  }
};

/**
 * Migrates all files from internal storage to external storage.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onFailure Failure callback.
 */
camera.models.FileSystem.migratePictures = function(onSuccess, onFailure) {
  var existByName = {};
  var internalEntriesByName = {};

  var migratePicture = function(pictureEntry, thumbnailEntry) {
    return new Promise(function(resolve, reject) {
      var fileName = camera.models.Gallery.regulateFileName(pictureEntry.name);
      var fileNameParts = camera.models.FileSystem.parseFileName_(fileName);
      var fileIndex = fileNameParts[2] + 1;
      while (existByName[fileName]) {
        fileName = fileNameParts[0] + ' (' + fileIndex + ')' + fileNameParts[1];
        fileIndex++;
      }
      pictureEntry.copyTo(
          camera.models.FileSystem.externalFs.root, fileName, function(newPicEntry) {
        if (fileName != pictureEntry.name && thumbnailEntry) {
          thumbnailEntry.moveTo(
              camera.models.FileSystem.internalFs.root,
              camera.models.Gallery.getThumbnailName(fileName));
        }
        // Remove the internal picture even failing to rename thumbnail.
        pictureEntry.remove(function() {});
        resolve();
      }, reject);
    });
  };

  camera.models.FileSystem.ackMigrate_();
  camera.models.FileSystem.reloadEntries_(function(internalEntries, externalEntries) {
    // Uses to get thumbnail entry.
    for (var index = 0; index < internalEntries.length; index++) {
      internalEntriesByName[internalEntries[index].name] =
          internalEntries[index];
    }

    // Uses to check whether file name conflict or not.
    for (var index = 0; index < externalEntries.length; index++) {
      existByName[externalEntries[index].name] = true;
    }

    var migrated = [];
    for (var index = 0; index < internalEntries.length; index++) {
      var entry = internalEntries[index];
      if (camera.models.FileSystem.isThumbnail_(entry))
        continue;

      var thumbnailName = camera.models.Gallery.getThumbnailName(entry.name);
      var thumbnailEntry = internalEntriesByName[thumbnailName];
      migrated.push(migratePicture(entry, thumbnailEntry));
    }
    Promise.all(migrated).then(function() {
      camera.models.FileSystem.doneMigrate_();
      onSuccess();
    }).
    catch(function(error) {
      console.error(error);
      onFailure('Failed to copy files to external storage.');
    });
  }, onFailure);
};

/**
 * Saves the blob to the given file name in the storage. Name of the actually
 * saved picture might be different from the given file name if the file
 * already exists.
 * @param {FileSystem} fileSystem File system to be written.
 * @param {string} fileName Name of the file in the storage.
 * @param {Blob} blob Data of the picture to be saved.
 * @param {function(FileEntry)} onSuccess Success callback with the entry of
 *     the saved picture.
 * @param {function(FileError)} onFailure Failure callback.
 * @private
 */
camera.models.FileSystem.saveToFile_ = function(
    fileSystem, fileName, blob, onSuccess, onFailure) {
  fileSystem.root.getFile(
      fileName,
      {create: true, exclusive: true},
      function(fileEntry) {
        fileEntry.createWriter(function(fileWriter) {
          fileWriter.onwriteend = function() {
            onSuccess(fileEntry);
          };
          fileWriter.onerror = onFailure;
          fileWriter.write(blob);
        },
        onFailure);
      }, function(error) {
        if (error instanceof DOMException &&
            error.name == "InvalidModificationError") {
          var fileNameParts = camera.models.FileSystem.parseFileName_(fileName);
          var fileIndex = fileNameParts[2] + 1;
          fileName = fileNameParts[0] + ' (' + fileIndex + ')' + fileNameParts[1];
          camera.models.FileSystem.saveToFile_(
              fileSystem, fileName, blob, onSuccess, onFailure);
        } else {
          onFailure();
        }
      });
};

/**
 * Saves the picture to the given file name in the storage. Name of the actually
 * saved picture might be different from the given file name if the file
 * already exists.
 * @param {string} fileName Name of the file in the storage.
 * @param {Blob} blob Data of the picture to be saved.
 * @param {function(FileEntry)} onSuccess Success callback with the entry of
 *     the saved picture.
 * @param {function(FileError)} onFailure Failure callback.
 */
camera.models.FileSystem.savePicture = function(
    fileName, blob, onSuccess, onFailure) {
  camera.models.FileSystem.saveToFile_(
      camera.models.FileSystem.externalFs ? camera.models.FileSystem.externalFs :
          camera.models.FileSystem.internalFs,
      fileName, blob, onSuccess, onFailure);
};

/**
 * Saves the thumbnail to the given file name in the storage. Name of the actually
 * saved thumbnail might be different from the given file name if the file
 * already exists.
 * @param {string} fileName Name of the file in the storage.
 * @param {Blob} blob Data of the picture to be saved.
 * @param {function(FileEntry)} onSuccess Success callback with the entry of
 *     the saved picture.
 * @param {function(FileError)} onFailure Failure callback.
 */
camera.models.FileSystem.saveThumbnail = function(
    fileName, blob, onSuccess, onFailure) {
  camera.models.FileSystem.saveToFile_(
      camera.models.FileSystem.internalFs, fileName, blob, onSuccess, onFailure);
};

/**
 * Returns true if entry is a thumbnail.
 * @param {FileSystemFileEntry} entry File entry.
 * @return {boolean} Is a thumbnail or not.
 * @private
 */
camera.models.FileSystem.isThumbnail_ = function(entry) {
  return (entry.name.indexOf(camera.models.Gallery.THUMBNAIL_PREFIX) === 0);
};

/**
 * Returns the picture and thumbnail entries.
 * @param {function(Array.<FileSystemFileEntry>, Array.<FileSystemFileEntry>)}
 *     onSuccess Success callback with all picture and thumbanil file entries.
 * @param {function()} onFailure Failure callback
 */
camera.models.FileSystem.getEntries = function(onSuccess, onFailure) {
  var isPicture = function(entry) {
    if (!entry.name.startsWith(camera.models.Gallery.VIDEO_PREFIX) &&
        !entry.name.startsWith(camera.models.Gallery.IMAGE_PREFIX)) {
      return false;
    }
    return entry.name.match(/_(\d{8})_(\d{6})(?: \((\d+)\))?/);
  };

  camera.models.FileSystem.reloadEntries_(function(internalEntries, externalEntries) {
    var pictureEntries = [];
    var thumbnailEntries = [];

    if (camera.models.FileSystem.externalFs) {
      for (var index = 0; index < externalEntries.length; index++) {
        if (isPicture(externalEntries[index])) {
          pictureEntries.push(externalEntries[index]);
        }
      }
      thumbnailEntries = internalEntries.filter(
          camera.models.FileSystem.isThumbnail_);
    } else {
      for (var index = 0; index < internalEntries.length; index++) {
        if (camera.models.FileSystem.isThumbnail_(internalEntries[index])) {
          thumbnailEntries.push(internalEntries[index]);
        } else {
          pictureEntries.push(internalEntries[index]);
        }
      }
    }
    onSuccess(pictureEntries, thumbnailEntries);
  }, onFailure);
};

/**
 * Creates and returns an URL for a picture.
 * @param {FileSystemFileEntry} entry File entry.
 * @param {function(string)} onSuccess Success callback with the url as a string.
 */
camera.models.FileSystem.pictureURL = function(entry, onSuccess) {
  if (!camera.models.FileSystem.externalFs) {
    onSuccess(entry.toURL());
  } else {
    entry.file(function(file) {
      let reader = new FileReader();
      reader.onload = function() {
        onSuccess(reader.result);
      };
      reader.readAsDataURL(file);
    });
  }
};

/**
 * Parses a file name.
 * @param {string} filename File name.
 * @return {Array.<string>} File name base, extension, and index.
 * @private
 */
camera.models.FileSystem.parseFileName_ = function(filename) {
  var base = '', ext = '', index = 0;
  var match = filename.match(/^([^.]+)(\..+)?$/);
  if (match) {
    base = match[1];
    ext = match[2];
    match = base.match(/ \((\d+)\)$/);
    if (match) {
      base = base.substring(0, match.index);
      index = parseInt(match[1], 10);
    }
  }
  return [base, ext, index];
};
