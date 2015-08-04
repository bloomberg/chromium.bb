// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class for enabling experimental features.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

(function () {

var kExperimentsStorageName = 'remoting-experiments';

/**
 * @param {Array.<string>} list
 */
function save(list) {
  var storageMap = {};
  storageMap[kExperimentsStorageName] = list;
  chrome.storage.local.set(storageMap);
};

/** @type {Object} */
remoting.experiments = {};

/**
 * Enables an experiment.
 *
 * @param {string} experiment to enable.
 */
remoting.experiments.enable = function(experiment) {
  remoting.experiments.get().then(function(/** Array.<string> */list) {
    if (list.indexOf(experiment) == -1) {
      list.push(experiment);
      save(list);
    }
  });
};

/**
 * Disables an experiment.
 *
 * @param {string} experiment to disable.
 */
remoting.experiments.disable = function(experiment) {
  remoting.experiments.get().then(function(/** Array.<string> */list) {
    list = list.filter(function(e) { return e !== experiment; });
    save(list);
  });
};

/**
 * Returns list of all enabled experiments.
 * @return {Promise}
 */
remoting.experiments.get = function() {
  return new Promise(function(resolve, reject) {
    chrome.storage.local.get(kExperimentsStorageName, function(items) {
      if (items.hasOwnProperty(kExperimentsStorageName)) {
        resolve(items[kExperimentsStorageName]);
      }
      resolve([]);
    });
  });
};

})();
