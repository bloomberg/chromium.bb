// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Installs a mock object to replace window.chrome in a unit test.
 * @param {Object} mockChrome
 */
function installMockChrome(mockChrome) {
  /** @suppress {const|checkTypes} */
  chrome = mockChrome;
}

/**
 * Mocks chrome.commandLinePrivate.
 * @constructor
 */
function MockCommandLinePrivate() {
  this.flags_ = {};
  if (!chrome) {
    installMockChrome({});
  }

  if (!chrome.commandLinePrivate) {
    /** @suppress {checkTypes} */
    chrome.commandLinePrivate = {};
  }
  chrome.commandLinePrivate.hasSwitch = (name, callback) => {
    window.setTimeout(() => {
      callback(name in this.flags_);
    }, 0);
  };
}

/**
 * Add a switch.
 * @param {string} name of the switch to add.
 */
MockCommandLinePrivate.prototype.addSwitch = function(name) {
  this.flags_[name] = true;
};

/**
 * Stubs the chrome.storage API.
 * @constructor
 * @struct
 */
function MockChromeStorageAPI() {
  /** @type {Object<?>} */
  this.state = {};

  /** @suppress {const} */
  window.chrome = window.chrome || {};
  /** @suppress {const} */
  window.chrome.runtime = window.chrome.runtime || {};  // For lastError.
  /** @suppress {checkTypes} */
  window.chrome.storage = {
    local: {
      get: this.get_.bind(this),
      set: this.set_.bind(this),
    }
  };
}

/**
 * @param {Array<string>|string} keys
 * @param {function(Object<?>)} callback
 * @private
 */
MockChromeStorageAPI.prototype.get_ = function(keys, callback) {
  var keys = keys instanceof Array ? keys : [keys];
  var result = {};
  keys.forEach((key) => {
    if (key in this.state) {
      result[key] = this.state[key];
    }
  });
  callback(result);
};

/**
 * @param {Object<?>} values
 * @param {function()=} opt_callback
 * @private
 */
MockChromeStorageAPI.prototype.set_ = function(values, opt_callback) {
  for (var key in values) {
    this.state[key] = values[key];
  }
  if (opt_callback) {
    opt_callback();
  }
};
