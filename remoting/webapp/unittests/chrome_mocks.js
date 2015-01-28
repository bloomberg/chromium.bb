// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various mock objects for the chrome platform to make
// unit testing easier.

Entry = function() {};

(function(scope){

var chromeMocks = {};

chromeMocks.Event = function() {
  this.listeners_ = [];
};

chromeMocks.Event.prototype.addListener = function(callback) {
  this.listeners_.push(callback);
};

chromeMocks.Event.prototype.removeListener = function(callback) {
  for (var i = 0; i < this.listeners_.length; i++) {
    if (this.listeners_[i] === callback) {
      this.listeners_.splice(i, 1);
      break;
    }
  }
};

chromeMocks.Event.prototype.mock$fire = function(var_args) {
  var params = Array.prototype.slice.call(arguments);
  this.listeners_.forEach(function(listener){
    listener.apply(null, params);
  });
};

chromeMocks.runtime = {};

chromeMocks.runtime.Port = function() {
  this.onMessage = new chromeMocks.Event();
  this.onDisconnect = new chromeMocks.Event();

  this.name = '';
  this.sender = null;
};

chromeMocks.runtime.Port.prototype.disconnect = function() {};
chromeMocks.runtime.Port.prototype.postMessage = function() {};

chromeMocks.runtime.onMessage = new chromeMocks.Event();
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

chromeMocks.runtime.id = 'extensionId';

chromeMocks.storage = {};

// Sample implementation of chrome.StorageArea according to
// https://developer.chrome.com/apps/storage#type-StorageArea
chromeMocks.StorageArea = function() {
  this.storage_ = {};
};

function getKeys(keys) {
  if (typeof keys === 'string') {
    return [keys];
  } else if (typeof keys === 'object') {
    return Object.keys(keys);
  }
  return [];
}

chromeMocks.StorageArea.prototype.get = function(keys, onDone) {
  if (!keys) {
    onDone(base.deepCopy(this.storage_));
    return;
  }

  var result = (typeof keys === 'object') ? keys : {};
  getKeys(keys).forEach(function(key) {
    if (key in this.storage_) {
      result[key] = base.deepCopy(this.storage_[key]);
    }
  }, this);
  onDone(result);
};

chromeMocks.StorageArea.prototype.set = function(value) {
  for (var key in value) {
    this.storage_[key] = base.deepCopy(value[key]);
  }
};

chromeMocks.StorageArea.prototype.remove = function(keys) {
  getKeys(keys).forEach(function(key) {
    delete this.storage_[key];
  }, this);
};

chromeMocks.StorageArea.prototype.clear = function() {
  this.storage_ = null;
};

chromeMocks.storage.local = new chromeMocks.StorageArea();

var originals_ = null;

/**
 * Activates a list of Chrome components to mock
 * @param {Array.<string>} components
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

scope.chromeMocks = chromeMocks;

})(window);
