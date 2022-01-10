// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileManagerPrivate API.

// Natives
var blobNatives = requireNative('blob_natives');
var fileManagerPrivateNatives = requireNative('file_manager_private');

// Internals
var fileManagerPrivateInternal = getInternalApi('fileManagerPrivateInternal');

// Shorthands
var GetFileSystem = fileManagerPrivateNatives.GetFileSystem;
var GetExternalFileEntry = fileManagerPrivateNatives.GetExternalFileEntry;

apiBridge.registerCustomHook(function(bindingsAPI) {
  // For FilesAppEntry types that wraps a native entry, returns the native entry
  // to be able to send to fileManagerPrivate API.
  function getEntryURL(entry) {
    const nativeEntry = entry.getNativeEntry && entry.getNativeEntry();
    if (nativeEntry)
      entry = nativeEntry;

    return fileManagerPrivateNatives.GetEntryURL(entry);
  }

  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('searchDrive',
      function(callback, response) {
    if (response && !response.error && response.entries) {
      response.entries = response.entries.map(function(entry) {
        return GetExternalFileEntry(entry);
      });
    }

    // So |callback| doesn't break if response is not defined.
    if (!response)
      response = {};

    if (callback)
      callback(response.entries, response.nextFeed);
  });

  apiFunctions.setCustomCallback('searchDriveMetadata',
      function(callback, response) {
    if (response && !response.error) {
      for (var i = 0; i < response.length; i++) {
        response[i].entry =
            GetExternalFileEntry(response[i].entry);
      }
    }

    // So |callback| doesn't break if response is not defined.
    if (!response)
      response = [];

    if (callback)
      callback(response);
  });

  apiFunctions.setHandleRequest('resolveIsolatedEntries',
                                function(entries, callback) {
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.resolveIsolatedEntries(urls, function(
        entryDescriptions) {
      callback(entryDescriptions.map(function(description) {
        return GetExternalFileEntry(description);
      }));
    });
  });

  apiFunctions.setHandleRequest('getVolumeRoot', function(options, callback) {
    fileManagerPrivateInternal.getVolumeRoot(options, function(entry) {
      callback(entry ? GetExternalFileEntry(entry) : undefined);
    });
  });

  apiFunctions.setHandleRequest('getEntryProperties',
                                function(entries, names, callback) {
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.getEntryProperties(urls, names, callback);
  });

  apiFunctions.setHandleRequest('addFileWatch', function(entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.addFileWatch(url, callback);
  });

  apiFunctions.setHandleRequest('removeFileWatch', function(entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.removeFileWatch(url, callback);
  });

  apiFunctions.setHandleRequest('getCustomActions', function(
        entries, callback) {
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.getCustomActions(urls, callback);
  });

  apiFunctions.setHandleRequest('executeCustomAction', function(
        entries, actionId, callback) {
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.executeCustomAction(urls, actionId, callback);
  });

  apiFunctions.setHandleRequest('computeChecksum', function(entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.computeChecksum(url, callback);
  });

  apiFunctions.setHandleRequest('getMimeType', function(entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.getMimeType(url, callback);
  });

  apiFunctions.setHandleRequest('getContentMimeType',
      function(fileEntry, callback) {
    fileEntry.file(blob => {
      var blobUUID = blobNatives.GetBlobUuid(blob);

      if (!blob || !blob.size) {
        callback(undefined);
        return;
      }

      var onGetContentMimeType = function(blob, mimeType) {
        callback(mimeType ? mimeType : undefined);
      }.bind(this, blob);  // Bind a blob reference: crbug.com/415792#c12

      fileManagerPrivateInternal.getContentMimeType(
          blobUUID, onGetContentMimeType);
    }, (error) => {
      var errorUUID = '';

      var onGetContentMimeType = function() {
        chrome.runtime.lastError.DOMError = /** @type {!DOMError} */ (error);
        callback(undefined);
      }.bind(this);

      fileManagerPrivateInternal.getContentMimeType(
          errorUUID, onGetContentMimeType);
    });
  });

  apiFunctions.setHandleRequest('getContentMetadata',
      function(fileEntry, mimeType, includeImages, callback) {
    fileEntry.file(blob => {
      var blobUUID = blobNatives.GetBlobUuid(blob);

      if (!blob || !blob.size) {
        callback(undefined);
        return;
      }

      var onGetContentMetadata = function(blob, metadata) {
        callback(metadata ? metadata : undefined);
      }.bind(this, blob);  // Bind a blob reference: crbug.com/415792#c12

      fileManagerPrivateInternal.getContentMetadata(
          blobUUID, mimeType, !!includeImages, onGetContentMetadata);
    }, (error) => {
      var errorUUID = '';

      var onGetContentMetadata = function() {
        chrome.runtime.lastError.DOMError = /** @type {!DOMError} */ (error);
        callback(undefined);
      }.bind(this);

      fileManagerPrivateInternal.getContentMetadata(
          errorUUID, mimeType, false, onGetContentMetadata);
    });
  });

  apiFunctions.setHandleRequest('pinDriveFile', function(entry, pin, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.pinDriveFile(url, pin, callback);
  });

  apiFunctions.setHandleRequest('executeTask',
      function(descriptor, entries, callback) {
        var urls = entries.map(function(entry) {
          return getEntryURL(entry);
        });
        fileManagerPrivateInternal.executeTask(descriptor, urls, callback);
      });

  apiFunctions.setHandleRequest('setDefaultTask',
      function(descriptor, entries, mimeTypes, callback) {
        var urls = entries.map(function(entry) {
          return getEntryURL(entry);
        });
        fileManagerPrivateInternal.setDefaultTask(
            descriptor, urls, mimeTypes, callback);
      });

  apiFunctions.setHandleRequest('getFileTasks', function(entries, callback) {
    var urls = entries.map(function(entry) {
      return getEntryURL(entry);
    });
    fileManagerPrivateInternal.getFileTasks(urls, callback);
  });

  apiFunctions.setHandleRequest('getDownloadUrl', function(entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.getDownloadUrl(url, callback);
  });

  apiFunctions.setHandleRequest(
      'getDisallowedTransfers', function(entries, destinationEntry, callback) {
        var sourceUrls = entries.map(getEntryURL);
        var destinationUrl = getEntryURL(destinationEntry);
        fileManagerPrivateInternal.getDisallowedTransfers(
            sourceUrls, destinationUrl,
            callback(entryDescriptions.map(function(description) {
              return GetExternalFileEntry(description);
            })));
      });

  apiFunctions.setHandleRequest('startCopy', function(
        entry, parentEntry, newName, callback) {
    var url = getEntryURL(entry);
    var parentUrl = getEntryURL(parentEntry);
    fileManagerPrivateInternal.startCopy(
        url, parentUrl, newName, callback);
  });

  apiFunctions.setHandleRequest(
      'zipSelection',
      (entries, parentEntry, destName, callback) =>
          fileManagerPrivateInternal.zipSelection(
              getEntryURL(parentEntry), entries.map(getEntryURL), destName,
              callback));

  apiFunctions.setHandleRequest('validatePathNameLength', function(
        entry, name, callback) {

    var url = getEntryURL(entry);
    fileManagerPrivateInternal.validatePathNameLength(url, name, callback);
  });

  apiFunctions.setHandleRequest('getDirectorySize', function(
        entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.getDirectorySize(url, callback);
  });

  apiFunctions.setHandleRequest('getRecentFiles', function(
        restriction, file_type, callback) {
    fileManagerPrivateInternal.getRecentFiles(restriction, file_type, function(
          entryDescriptions) {
      callback(entryDescriptions.map(function(description) {
        return GetExternalFileEntry(description);
      }));
    });
  });

  apiFunctions.setHandleRequest(
      'sharePathsWithCrostini', function(vmName, entries, persist, callback) {
        const urls = entries.map((entry) => {
          return getEntryURL(entry);
        });
        fileManagerPrivateInternal.sharePathsWithCrostini(
            vmName, urls, persist, callback);
      });

  apiFunctions.setHandleRequest(
      'unsharePathWithCrostini', function(vmName, entry, callback) {
        fileManagerPrivateInternal.unsharePathWithCrostini(
            vmName, getEntryURL(entry), callback);
      });

  apiFunctions.setHandleRequest(
      'getCrostiniSharedPaths',
      function(observeFirstForSession, vmName, callback) {
        fileManagerPrivateInternal.getCrostiniSharedPaths(
            observeFirstForSession, vmName,
            function(entryDescriptions, firstForSession) {
              callback(entryDescriptions.map(function(description) {
                return GetExternalFileEntry(description);
              }), firstForSession);
            });
      });

  apiFunctions.setHandleRequest(
      'getLinuxPackageInfo', function(entry, callback) {
        var url = getEntryURL(entry);
        fileManagerPrivateInternal.getLinuxPackageInfo(url, callback);
      });

  apiFunctions.setHandleRequest('installLinuxPackage', function(
        entry, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.installLinuxPackage(url, callback);
  });

  apiFunctions.setHandleRequest('getDriveThumbnail', function(
        entry, cropToSquare, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.getDriveThumbnail(url, cropToSquare, callback);
  });

  apiFunctions.setHandleRequest('getPdfThumbnail', function(
        entry, width, height, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.getPdfThumbnail(url, width, height, callback);
  });

  apiFunctions.setHandleRequest('getArcDocumentsProviderThumbnail', function(
        entry, widthHint, heightHint, callback) {
    var url = getEntryURL(entry);
    fileManagerPrivateInternal.getArcDocumentsProviderThumbnail(
        url, widthHint, heightHint, callback);
  });

  apiFunctions.setCustomCallback('searchFiles',
      function(callback, response) {
    if (response && !response.error && response.entries) {
      response.entries = response.entries.map(function(entry) {
        return GetExternalFileEntry(entry);
      });
    }

    // So |callback| doesn't break if response is not defined.
    if (!response) {
      response = {};
    }

    if (callback) {
      callback(response.entries);
    }
  });

  apiFunctions.setHandleRequest('importCrostiniImage', function(entry) {
    const url = getEntryURL(entry);
    fileManagerPrivateInternal.importCrostiniImage(url);
  });

  apiFunctions.setHandleRequest(
      'sharesheetHasTargets', function(entries, callback) {
        var urls = entries.map(function(entry) {
          return getEntryURL(entry);
        });
        fileManagerPrivateInternal.sharesheetHasTargets(urls, callback);
      });

  apiFunctions.setHandleRequest(
      'invokeSharesheet', function(entries, launchSource, callback) {
        var urls = entries.map(function(entry) {
          return getEntryURL(entry);
        });
        fileManagerPrivateInternal.invokeSharesheet(
            urls, launchSource, callback);
      });

  apiFunctions.setHandleRequest(
      'toggleAddedToHoldingSpace', function(entries, added, callback) {
        const urls = entries.map(entry => getEntryURL(entry));
        fileManagerPrivateInternal.toggleAddedToHoldingSpace(
            urls, added, callback);
      });

  apiFunctions.setHandleRequest('startIOTask', function(type, entries, params) {
    const urls = entries.map(entry => getEntryURL(entry));
    let newParams = {};
    if (params.destinationFolder) {
      newParams.destinationFolderUrl = getEntryURL(params.destinationFolder);
    }
    fileManagerPrivateInternal.startIOTask(type, urls, newParams);
  });
});

bindingUtil.registerEventArgumentMassager(
    'fileManagerPrivate.onDirectoryChanged', function(args, dispatch) {
  // Convert the entry arguments into a real Entry object.
  args[0].entry = GetExternalFileEntry(args[0].entry);
  dispatch(args);
});

bindingUtil.registerEventArgumentMassager(
    'fileManagerPrivate.onCrostiniChanged', function(args, dispatch) {
  // Convert entries arguments into real Entry objects.
  const entries = args[0].entries;
  for (let i = 0; i < entries.length; i++) {
    entries[i] = GetExternalFileEntry(entries[i]);
  }
  dispatch(args);
});
