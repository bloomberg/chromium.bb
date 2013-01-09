// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Mock class for Chrome's storage API to aid with the transition to Apps v2.
 *
 * The local storage API is not supported in Apps v2, so we'll have to use
 * the new storage API instead. This doesn't require Apps v2, but it turns
 * out that migrating our OAuth2 code to the new API is non-trivial; since
 * that code will need to be rewritten to be Apps-v2-compatible anyway, a
 * mock seems like a good temporary solution.
 *
 * TODO(jamiewalch): Delete this file when we migrate to apps v2
 * (http://crbug.com/ 134213).
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

remoting.initMockStorage = function() {
  if (typeof(chrome.storage) != 'undefined') {
    console.warn('Congratulations! You already have access to chrome.storage.',
                 'You should probably remove the call to initMockStorage.');
  } else {
    /** @type {Object} */
    chrome.storage = {};
    chrome.storage.local = new remoting.MockStorage();
  }
};

/**
 * @constructor
 */
remoting.MockStorage = function() {
};

/** @type {number} Delay for |get|, in ms, to simulate slow local storage
 *      and test correct asynchronous behavior. A value of 0 results in
 *      synchronous behavior.
 */
remoting.MockStorage.GET_DELAY = 0;

/**
 * @param {string|Array.<string>|Object.<string>} items
 * @param {function(Object.<string>):void} callback
 * @return {void}
 */
remoting.MockStorage.prototype.get = function(items, callback) {
  var result = {};
  if (items == null) {
    // Return all items for null input.
    result = window.localStorage;

  } else if (typeof(items) == 'string') {
    // If the input is a string, return one or zero items, depending on
    // whether or not the value is stored in localStorage.
    if (items in window.localStorage) {
      result[items] = window.localStorage.getItem(items);
    }

  } else if (items.constructor == Array) {
    // If the input is an array, return those items that have an entry
    // in localStorage.
    for (var index in items) {
      /** @type {string} */
      var item = items[index];
      if (item in window.localStorage) {
        result[item] = window.localStorage.getItem(item);
      }
    }

  } else {
    // If the input is a dictionary, return the localStorage value for
    // items that are stored, and the dictionary value otherwise.
    for (var item in items) {
      if (item in window.localStorage) {
        result[item] = window.localStorage.getItem(item);
      } else {
        result[item] = items[item];
      }
    }
  }

  if (remoting.MockStorage.GET_DELAY == 0) {
    callback(result);
  } else {
    window.setTimeout(callback.bind(null, result),
                      remoting.MockStorage.GET_DELAY);
  }
};

/**
 * @param {Object.<string>} items
 * @param {function():void=} opt_callback
 * @return {void}
 */
remoting.MockStorage.prototype.set = function(items, opt_callback) {
  for (var item in items) {
    window.localStorage.setItem(item, items[item]);
  }
  if (opt_callback) {
    opt_callback();
  }
};

/**
 * @param {string|Array.<string>} items
 * @param {function():void=} opt_callback
 * @return {void}
 */
remoting.MockStorage.prototype.remove = function(items, opt_callback) {
  if (typeof(items) == 'string') {
    window.localStorage.removeItem(items);
  } else if (items.constructor == Array) {
    for (var index in items) {
      window.localStorage.removeItem(items[index]);
    }
  }
  if (opt_callback) {
    opt_callback();
  }
};

/**
 * @param {function():void=} opt_callback
 * @return {void}
 */
remoting.MockStorage.prototype.clear = function(opt_callback) {
  window.localStorage.clear();
  if (opt_callback) {
    opt_callback();
  }
};