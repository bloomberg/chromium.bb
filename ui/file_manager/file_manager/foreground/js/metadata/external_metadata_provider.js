// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   size: number,
 *   shared: (boolean|undefined),
 *   modificationTime: Date,
 *   thumbnailUrl: (string|undefined),
 *   externalFileUrl: (string|undefined),
 *   imageWidth: (number|undefined),
 *   imageHeight: (number|undefined),
 *   imageRotation: (number|undefined),
 *   pinned: (boolean|undefined),
 *   present: (boolean|undefined),
 *   hosted: (boolean|undefined),
 *   dirty: (boolean|undefined),
 *   availableOffline: (boolean|undefined),
 *   availableWhenMetered: (boolean|undefined),
 *   customIconUrl: string,
 *   contentMimeType: string,
 *   sharedWithMe: (boolean|undefined)
 * }}
 */
var ExternalMetadata;

/**
 * Metadata provider for FileEntry#getMetadata.
 *
 * @param {!MetadataProviderCache} cache
 * @constructor
 * @extends {NewMetadataProvider<!ExternalMetadata>}
 * @struct
 */
function ExternalMetadataProvider(cache) {
  NewMetadataProvider.call(this, cache, [
    'availableOffline',
    'availableWhenMetered',
    'contentMimeType',
    'customIconUrl',
    'dirty',
    'externalFileUrl',
    'hosted',
    'imageHeight',
    'imageRotation',
    'imageWidth',
    'modificationTime',
    'pinned',
    'present',
    'shared',
    'sharedWithMe',
    'size',
    'thumbnailUrl'
  ]);
}

ExternalMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

/**
 * @override
 */
ExternalMetadataProvider.prototype.getImpl = function(requests) {
  return new Promise(function(fulfill, reject) {
    var urls = [];
    for (var i = 0; i < requests.length; i++) {
      urls.push(requests[i].entry.toURL());
    }
    chrome.fileManagerPrivate.getEntryProperties(
        urls,
        function(results) {
          if (!chrome.runtime.lastError)
            fulfill(this.convertResults_(requests, results));
          else
            reject(chrome.runtime.lastError);
        }.bind(this));
  }.bind(this));
};

/**
 * @param {!Array<!MetadataRequest>} requests
 * @param {!Array<!EntryProperties>} propertiesList
 * @return {!Array<!ExternalMetadata>}
 */
ExternalMetadataProvider.prototype.convertResults_ =
    function(requests, propertiesList) {
  var results = [];
  for (var i = 0; i < propertiesList.length; i++) {
    var properties = propertiesList[i];
    results.push({
      availableOffline: properties.isAvailableOffline,
      availableWhenMetered: properties.isAvailableWhenMetered,
      contentMimeType: properties.contentMimeType || '',
      customIconUrl: properties.customIconUrl || '',
      dirty: properties.isDirty,
      externalFileUrl: properties.externalFileUrl,
      hosted: properties.isHosted,
      imageHeight: properties.imageHeight,
      imageRotation: properties.imageRotation,
      imageWidth: properties.imageWidth,
      modificationTime: new Date(properties.lastModifiedTime),
      pinned: properties.isPinned,
      present: properties.isPresent,
      shared: properties.shared,
      sharedWithMe: properties.sharedWithMe,
      size: requests[i].entry.isFile ? (properties.fileSize || 0) : -1,
      thumbnailUrl: properties.thumbnailUrl
    });
  }
  return results;
};
