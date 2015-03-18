// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various mock objects for the chrome platform to make
// unit testing easier.

Entry = function() {};

var chromeMocks = {};

/** @constructor */
chrome.Event = function() {};

/** @param {Function} callback */
chrome.Event.prototype.addListener = function(callback) {};

/** @param {Function} callback */
chrome.Event.prototype.removeListener = function(callback) {};


(function(){

/**
 * @constructor
 * @extends {chrome.Event}
 */
chromeMocks.Event = function() {
  this.listeners_ = [];
};

/** @param {Function} callback */
chromeMocks.Event.prototype.addListener = function(callback) {
  this.listeners_.push(callback);
};

/** @param {Function} callback */
chromeMocks.Event.prototype.removeListener = function(callback) {
  for (var i = 0; i < this.listeners_.length; i++) {
    if (this.listeners_[i] === callback) {
      this.listeners_.splice(i, 1);
      break;
    }
  }
};

/**
 * @param {...*} var_args
 * @return {void}
 */
chromeMocks.Event.prototype.mock$fire = function(var_args) {
  var params = Array.prototype.slice.call(arguments);
  this.listeners_.forEach(
      /** @param {Function} listener */
      function(listener){
        listener.apply(null, params);
      });
};

/** @type {Object} */
chromeMocks.runtime = {};

/** @constructor */
chromeMocks.runtime.Port = function() {
  this.onMessage = new chromeMocks.Event();
  this.onDisconnect = new chromeMocks.Event();

  /** @type {string} */
  this.name = '';

  /** @type {chrome.runtime.MessageSender} */
  this.sender = null;
};

chromeMocks.runtime.Port.prototype.disconnect = function() {};

/**
 * @param {Object} message
 */
chromeMocks.runtime.Port.prototype.postMessage = function(message) {};

/** @type {chromeMocks.Event} */
chromeMocks.runtime.onMessage = new chromeMocks.Event();

/**
 * @param {string?} extensionId
 * @param {*} message
 * @param {function(*)=} responseCallback
 */
chromeMocks.runtime.sendMessage = function(extensionId, message,
                                           responseCallback) {
  base.debug.assert(
      extensionId === null,
      'The mock only supports sending messages to the same extension.');
  extensionId = chrome.runtime.id;
  window.requestAnimationFrame(function() {
    var message_copy = base.deepCopy(message);
    chromeMocks.runtime.onMessage.mock$fire(
        message_copy, {id: extensionId}, responseCallback);
  });
};

/** @type {string} */
chromeMocks.runtime.id = 'extensionId';

/** @type {Object} */
chromeMocks.runtime.lastError = {
  /** @type {string|undefined} */
  message: undefined
};


// Sample implementation of chrome.StorageArea according to
// https://developer.chrome.com/apps/storage#type-StorageArea
/** @constructor */
chromeMocks.StorageArea = function() {
  /** @type {Object} */
  this.storage_ = {};
};

/**
 * @param {!Object} keys
 * @return {Array<string>}
 */
function getKeys(keys) {
  if (typeof keys === 'string') {
    return [keys];
  } else if (typeof keys === 'object') {
    return Object.keys(keys);
  }
  return [];
}

/**
 * @param {!Object} keys
 * @param {Function} onDone
 */
chromeMocks.StorageArea.prototype.get = function(keys, onDone) {
  if (!keys) {
    onDone(base.deepCopy(this.storage_));
    return;
  }

  var result = (typeof keys === 'object') ? keys : {};
  getKeys(keys).forEach(
      /** @param {string} key */
      function(key) {
        if (key in this.storage_) {
          result[key] = base.deepCopy(this.storage_[key]);
        }
      }, this);
  onDone(result);
};

/** @param {Object} value */
chromeMocks.StorageArea.prototype.set = function(value) {
  for (var key in value) {
    this.storage_[key] = base.deepCopy(value[key]);
  }
};

/**
 * @param {!Object} keys
 */
chromeMocks.StorageArea.prototype.remove = function(keys) {
  getKeys(keys).forEach(
      /** @param {string} key */
      function(key) {
        delete this.storage_[key];
      }, this);
};

chromeMocks.StorageArea.prototype.clear = function() {
  this.storage_ = null;
};

/** @type {Object} */
chromeMocks.storage = {};

/** @type {chromeMocks.StorageArea} */
chromeMocks.storage.local = new chromeMocks.StorageArea();


/** @constructor */
chromeMocks.Identity = function() {
  /** @private {string|undefined} */
  this.token_ = undefined;
};

/**
 * @param {Object} options
 * @param {function(string=):void} callback
 */
chromeMocks.Identity.prototype.getAuthToken = function(options, callback) {
  // Append the 'scopes' array, if present, to the dummy token.
  var token = this.token_;
  if (token !== undefined && options['scopes'] !== undefined) {
    token += JSON.stringify(options['scopes']);
  }
  // Don't use setTimeout because sinon mocks it.
  window.requestAnimationFrame(callback.bind(null, token));
};

/** @param {string} token */
chromeMocks.Identity.prototype.mock$setToken = function(token) {
  this.token_ = token;
};

chromeMocks.Identity.prototype.mock$clearToken = function() {
  this.token_ = undefined;
};

/** @type {chromeMocks.Identity} */
chromeMocks.identity = new chromeMocks.Identity();


var originals_ = null;

/**
 * Activates a list of Chrome components to mock
 * @param {Array<string>} components
 */
chromeMocks.activate = function(components) {
  if (originals_) {
    throw new Error('chromeMocks.activate() can only be called once.');
  }
  originals_ = {};
  components.forEach(function(component) {
    if (!chromeMocks[component]) {
      throw new Error('No mocks defined for chrome.' + component);
    }
    originals_[component] = chrome[component];
    chrome[component] = chromeMocks[component];
  });
};

chromeMocks.restore = function() {
  if (!originals_) {
    throw new Error('You must call activate() before restore().');
  }
  for (var components in originals_) {
    chrome[components] = originals_[components];
  }
  originals_ = null;
};

})();
