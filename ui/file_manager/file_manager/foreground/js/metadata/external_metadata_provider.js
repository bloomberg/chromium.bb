// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Metadata provider for FileEntry#getMetadata.
 * TODO(hirono): Rename thumbnailUrl with externalThumbnailUrl.
 *
 * @constructor
 * @extends {NewMetadataProvider}
 * @struct
 */
function ExternalMetadataProvider() {
  NewMetadataProvider.call(this, ExternalMetadataProvider.PROPERTY_NAMES);
}

/**
 * @const {!Array<string>}
 */
ExternalMetadataProvider.PROPERTY_NAMES = [
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
];

ExternalMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

/**
 * @override
 */
ExternalMetadataProvider.prototype.get = function(requests) {
  if (!requests.length)
    return Promise.resolve([]);
  return new Promise(function(fulfill) {
    var urls = [];
    for (var i = 0; i < requests.length; i++) {
      urls.push(requests[i].entry.toURL());
    }
    var nameMap = [];
    for (var i = 0; i < requests.length; i++) {
      for (var j = 0; j < requests[i].names.length; j++) {
        nameMap[requests[i].names[j]] = true;
      }
    }
    chrome.fileManagerPrivate.getEntryProperties(
        urls,
        Object.keys(nameMap),
        function(results) {
          if (!chrome.runtime.lastError)
            fulfill(this.convertResults_(requests, results));
          else
            fulfill(requests.map(function() { return new MetadataItem(); }));
        }.bind(this));
  }.bind(this));
};

/**
 * @param {!Array<!MetadataRequest>} requests
 * @param {!Array<!EntryProperties>} propertiesList
 * @return {!Array<!MetadataItem>}
 */
ExternalMetadataProvider.prototype.convertResults_ =
    function(requests, propertiesList) {
  var results = [];
  for (var i = 0; i < propertiesList.length; i++) {
    var properties = propertiesList[i];
    var item = new MetadataItem();
    item.availableOffline = properties.availableOffline;
    item.availableWhenMetered = properties.availableWhenMetered;
    item.contentMimeType = properties.contentMimeType || '';
    item.customIconUrl = properties.customIconUrl || '';
    item.dirty = properties.dirty;
    item.externalFileUrl = properties.externalFileUrl;
    item.hosted = properties.hosted;
    item.imageHeight = properties.imageHeight;
    item.imageRotation = properties.imageRotation;
    item.imageWidth = properties.imageWidth;
    item.modificationTime = new Date(properties.modificationTime);
    item.pinned = properties.pinned;
    item.present = properties.present;
    item.shared = properties.shared;
    item.sharedWithMe = properties.sharedWithMe;
    item.size = requests[i].entry.isFile ? (properties.size || 0) : -1;
    item.thumbnailUrl = properties.thumbnailUrl;
    results.push(item);
  }
  return results;
};
