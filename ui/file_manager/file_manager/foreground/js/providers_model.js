// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * An item in the model. Represents a single providing extension.
 *
 * @param {string} extensionId
 * @param {string} extensionName
 * @param {boolean} configurable
 * @param {boolean} watchable
 * @param {boolean} multipleMounts
 * @param {string} source
 * @constructor
 * @struct
 */
function ProvidersModelItem(
    extensionId, extensionName, configurable, watchable, multipleMounts,
    source) {
  /**
   * @private {string}
   * @const
   */
  this.extensionId_ = extensionId;

  /**
   * @private {string}
   * @const
   */
  this.extensionName_ = extensionName;

  /**
   * @private {boolean}
   * @const
   */
  this.configurable_ = configurable;

  /**
   * @private {boolean}
   * @const
   */
  this.watchable_ = watchable;

  /**
   * @private {boolean}
   * @const
   */
  this.multipleMounts_ = multipleMounts;

  /**
   * @private {string}
   * @const
   */
  this.source_ = source;
}

ProvidersModelItem.prototype = {
  /**
   * @return {string}
   */
  get extensionId() { return this.extensionId_; },

  /**
   * @return {string}
   */
  get extensionName() { return this.extensionName_; },

  /**
   * @return {boolean}
   */
  get configurable() { return this.configurable_; },

  /**
   * @return {boolean}
   */
  get watchable() { return this.watchable_; },

  /**
   * @return {boolean}
   */
  get multipleMounts() { return this.multipleMounts_; },

  /**
   * @return {string}
   */
  get source() { return this.source_; }
};

/**
 * Model for providing extensions. Providers methods for fetching lists of
 * providing extensions as well as performing operations on them, such as
 * requesting a new mount point.
 *
 * @param {!VolumeManagerWrapper} volumeManager
 * @constructor
 * @struct
 */
function ProvidersModel(volumeManager) {
  /**
   * @private {!VolumeManagerWrapper}
   * @const
   */
  this.volumeManager_ = volumeManager;
}

/**
 * @return {!Promise<Array<ProvidersModelItem>>}
 */
ProvidersModel.prototype.getInstalledProviders = function() {
  return new Promise(
      function(fulfill, reject) {
        chrome.fileManagerPrivate.getProvidingExtensions(function(extensions) {
          if (chrome.runtime.lastError) {
            reject(chrome.runtime.lastError.message);
            return;
          }
          var results = [];
          extensions.forEach(function(extension) {
            results.push(new ProvidersModelItem(
                extension.extensionId,
                extension.name,
                extension.configurable,
                extension.watchable,
                extension.multipleMounts,
                extension.source));
          });
          fulfill(results);
        });
      });
};

/**
 * @return {!Promise<Array<ProvidersModelItem>>}
 */
ProvidersModel.prototype.getMountableProviders = function() {
  return this.getInstalledProviders().then(function(extensions) {
    var mountedProviders = {};
    for (var i = 0; i < this.volumeManager_.volumeInfoList.length; i++) {
      var volumeInfo = this.volumeManager_.volumeInfoList.item(i);
      if (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.PROVIDED)
        mountedProviders[volumeInfo.extensionId] = true;
    }
    return extensions.filter(function(item) {
      // File systems handling files are mounted via file handlers. Device
      // handlers are mounted when a device is inserted. Only network file
      // systems are mounted manually by user via a menu.
      return item.source === 'network' &&
          (!mountedProviders[item.extensionId] || item.multipleMounts);
    });
  }.bind(this));
};

/**
 * @param {string} extensionId
 */
ProvidersModel.prototype.requestMount = function(extensionId) {
  chrome.fileManagerPrivate.addProvidedFileSystem(
      assert(extensionId),
      function() {
        if (chrome.runtime.lastError)
            console.error(chrome.runtime.lastError.message);
      });
};
