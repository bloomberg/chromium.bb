// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class handling saving and restoring of per-host options.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @type {Object} */
remoting.HostSettings = {};

/**
 * Load the settings for the specified host. Settings are returned as a
 * dictionary of (name, value) pairs.
 *
 * @param {string} hostId The host identifer for which to load options.
 * @param {function(Object):void} callback Callback to which the
 *     current settings for the host are passed. If there are no settings,
 *     then an empty dictionary is passed.
 * @return {void} Nothing.
 */
remoting.HostSettings.load = function(hostId, callback) {
  /**
   * @param {Object} requestedHost
   * @param {Object} allHosts
   * @return {void} Nothing.
   */
  var onDone = function(requestedHost, allHosts) {
    callback(requestedHost);
  };
  remoting.HostSettings.loadInternal_(hostId, onDone);
};

/**
 * Save the settings for the specified hosts. Existing settings with the same
 * names will be overwritten; settings not currently saved will be created.
 *
 * @param {string} hostId The host identifer for which to save options.
 * @param {Object} options The options to save, expressed as a dictionary of
 *     (name, value) pairs.
 * @param {function():void=} opt_callback Optional completion callback.
 * @return {void} Nothing.
 */
remoting.HostSettings.save = function(hostId, options, opt_callback) {
  /**
   * @param {Object} requestedHost
   * @param {Object} allHosts
   * @return {void} Nothing.
   */
  var onDone = function(requestedHost, allHosts) {
    for (var option in options) {
      requestedHost[option] = options[option];
    }
    allHosts[hostId] = requestedHost;
    var newSettings = {};
    newSettings[remoting.HostSettings.KEY_] = JSON.stringify(allHosts);
    chrome.storage.local.set(newSettings, opt_callback);
  };
  remoting.HostSettings.loadInternal_(hostId, onDone);
};

/**
 * Helper function for both load and save.
 *
 * @param {string} hostId The host identifer for which to load options.
 * @param {function(Object, Object):void} callback Callback to which the
 *     current settings for the specified host and for all hosts are passed.
 * @return {void} Nothing.
 */
remoting.HostSettings.loadInternal_ = function(hostId, callback) {
  /**
   * @param {Object.<string>} allHosts The current options for all hosts.
   * @return {void} Nothing.
   */
  var onDone = function(allHosts) {
    var result = {};
    try {
      result = jsonParseSafe(allHosts[remoting.HostSettings.KEY_]);
      if (typeof(result) != 'object') {
        console.error("Error loading host settings: Not an object");
        result = {};
      } else if (hostId in result &&
                 typeof(result[hostId]) == 'object') {
        callback(result[hostId], /** @type {Object} */(result));
        return;
      }
    } catch (err) {
      var typedErr = /** @type {*} */ (err);
      console.error('Error loading host settings:', typedErr);
    }
    callback({}, /** @type {Object} */(result));
  };
  chrome.storage.local.get(remoting.HostSettings.KEY_, onDone);
};

/** @type {string} @private */
remoting.HostSettings.KEY_ = 'remoting-host-options';