// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Discover the ID of installed cast extesnion.
 * @constructor
 */
function CastExtensionDiscoverer() {
}

/**
 * Tentatice IDs to try.
 * @type {Array.<string>}
 * @const
 */
CastExtensionDiscoverer.CAST_EXTENSION_IDS = [
  'boadgeojelhgndaghljhdicfkmllpafd', // release
  'dliochdbjfkdbacpmhlcpmleaejidimm', // beta
  'hfaagokkkhdbgiakmmlclaapfelnkoah',
  'fmfcbgogabcbclcofgocippekhfcmgfj',
  'enhhojjnijigcajfphajepfemndkmdlo'
];

/**
 * @param {function(string)} callback Callback called with the extension ID. The
 *     ID may be null if extension is not found.
 */
CastExtensionDiscoverer.findInstalledExtension = function(callback) {
  CastExtensionDiscoverer.findInstalledExtensionHelper_(0, callback);
};

/**
 * @param {number} index Current index which is tried to check.
 * @param {function(string)} callback Callback function which will be called
 *     the extension is found.
 */
CastExtensionDiscoverer.findInstalledExtensionHelper_ = function(index,
    callback) {
  if (index === CastExtensionDiscoverer.CAST_EXTENSION_IDS.length) {
    // no extension found.
    callback(null);
    return;
  }

  CastExtensionDiscoverer.isExtensionInstalled_(
      CastExtensionDiscoverer.CAST_EXTENSION_IDS[index],
      function(installed) {
        if (installed) {
          callback(CastExtensionDiscoverer.CAST_EXTENSION_IDS[index]);
        } else {
          CastExtensionDiscoverer.findInstalledExtensionHelper_(index + 1,
                                                                callback);
        }
      });
};

/**
 * The result will be notified on |callback|. True if installed, false not.
 * @param {string} extensionId Id to be checked.
 * @param {function(boolean)} callback Callback to notify the result.
 */
CastExtensionDiscoverer.isExtensionInstalled_ =
    function(extensionId, callback) {

  var responseCallback =
      /** @param {*} response */
      function(response) {
        if (chrome.runtime.lastError || response === false) {
          // An error occurred while sending the message.
          callback(false);
        } else {
          // Cast extension found.
          callback(true);
        }
      }.wrap(this);

  chrome.runtime.sendMessage(extensionId, {}, responseCallback);
};
