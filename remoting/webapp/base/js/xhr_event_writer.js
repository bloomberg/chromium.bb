// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * A class that writes log events to our back-end using XHR.
 * If a log request fails due to network failure, it will be stored to
 * |storage| for retrying in the future by calling flush().
 *
 * @param {!string} url
 * @param {!StorageArea} storage
 * @param {!string} storageKey
 * @constructor
 */
remoting.XhrEventWriter = function(url, storage, storageKey) {
  /** @private */
  this.url_ = url;
  /** @private */
  this.storage_ = storage;
  /** @private @const */
  this.storageKey_ = storageKey;
  /** @private */
  this.pendingRequests_ = new Map();
};

/**
 * @return {Promise} A promise that resolves when initialization is completed.
 */
remoting.XhrEventWriter.prototype.loadPendingRequests = function() {
  var that = this;
  var deferred = new base.Deferred();
  this.storage_.get(this.storageKey_, function(entry) {
    try {
      that.pendingRequests_ = new Map(entry[that.storageKey_]);
    } catch(e) {
      that.pendingRequests_ = new Map();
    }
    deferred.resolve(that.pendingRequests_);
  });
  return deferred.promise();
};

/**
 * @param {Object} event  The event to be written to the server.
 * @return {Promise} A promise that resolves on success.
 */
remoting.XhrEventWriter.prototype.write = function(event) {
  this.markPending_(event);
  return this.flush();
};

/**
 * @return {Promise} A promise that resolves on success.
 */
remoting.XhrEventWriter.prototype.flush = function() {
  var promises = /** @type {!Array<!Promise>} */ ([]);
  var that = this;
  // Map.forEach enumerates the entires of the map in insertion order.
  this.pendingRequests_.forEach(
    function(/** Object */ event, /** string */ requestId) {
      promises.push(that.doXhr_(requestId, event));
    });
  return Promise.all(promises);
};

/**
 * @return {Promise} A promise that resolves when the pending requests are
 *     written to disk.
 */
remoting.XhrEventWriter.prototype.writeToStorage = function() {
  var deferred = new base.Deferred();
  var map = [];
  this.pendingRequests_.forEach(
    function(/** Object */ request, /** string */ id) {
      map.push([id, request]);
    });

  var entry = {};
  entry[this.storageKey_] = map;
  this.storage_.set(entry, deferred.resolve.bind(deferred));
  return deferred.promise();
};

/**
 * @param {string} requestId
 * @param {Object} event
 * @return {Promise}
 * @private
 */
remoting.XhrEventWriter.prototype.doXhr_ = function(requestId, event) {
  var that = this;
  var XHR_RETRY_ATTEMPTS = 20;
  var xhr = new remoting.AutoRetryXhr(
      {method: 'POST', url: this.url_, jsonContent: event}, XHR_RETRY_ATTEMPTS);
  return xhr.start().then(function(response) {
    var error = remoting.Error.fromHttpStatus(response.status);
    // Only store requests that are failed with NETWORK_FAILURE, so that
    // malformed requests won't be stuck in the client forever.
    if (!error.hasTag(remoting.Error.Tag.NETWORK_FAILURE)) {
      that.pendingRequests_.delete(requestId);
    }
    if (!error.isNone()) {
      throw error;
    }
  });
};

/**
 * @param {number} length
 * @return {string} A random string of length |length|
 */
function randomString(length) {
  var random = new Uint8Array(length);
  window.crypto.getRandomValues(random);
  return window.btoa(String.fromCharCode.apply(null, random));
}

/**
 * @param {Object} event
 * @private
 */
remoting.XhrEventWriter.prototype.markPending_ = function(event) {
  var requestId = Date.now() + '_' + randomString(16);
  base.debug.assert(!this.pendingRequests_.has(requestId));
  this.pendingRequests_.set(requestId, event);
};

})();
