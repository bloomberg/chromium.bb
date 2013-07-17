// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Extract the appropriate fields from the input parameter, if present. Use the
 * isValid() method to determine whether or not a valid paired client instance
 * was provided.
 *
 * @param {Object} pairedClient The paired client, as returned by the native
 *     host instance.
 * @constructor
 */
remoting.PairedClient = function(pairedClient) {
  if (!pairedClient || typeof(pairedClient) != 'object') {
    return;
  }

  this.clientId = /** @type {string} */ (pairedClient['clientId']);
  this.clientName = /** @type {string} */ (pairedClient['clientName']);
  this.createdTime = /** @type {number} */ (pairedClient['createdTime']);
};

/**
 * @return {boolean} True if the constructor parameter was a well-formed
 *     paired client instance.
 */
remoting.PairedClient.prototype.isValid = function() {
  return typeof(this.clientId) == 'string' &&
         typeof(this.clientName) == 'string' &&
         typeof(this.createdTime) == 'number';
};

/**
 * Converts a raw object to an array of PairedClient instances. Returns null if
 * the input object is incorrectly formatted.
 *
 * @param {*} pairedClients The object to convert.
 * @return {Array.<remoting.PairedClient>} The converted result.
 */
remoting.PairedClient.convertToPairedClientArray = function(pairedClients) {
  if (!(pairedClients instanceof Array)) {
    console.error('pairedClients is not an Array: ' +
                  pairedClients);
    return null;
  }

  var result = [];
  for (var i = 0; i < pairedClients.length; i++) {
    var pairedClient = new remoting.PairedClient(pairedClients[i]);
    if (!pairedClient.isValid()) {
      console.error('pairedClient[' + i + '] has incorrect format:',
                    /** @type {*} */(pairedClients[i]));
      return null;
    }
    result.push(pairedClient);
  }
  return result;
}
