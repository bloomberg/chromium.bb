// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This hack prevents a bug on the cast extension.
// TODO(yoshiki): Remove this once the cast extension supports Chrome apps.
// Although localStorage in Chrome app is not supported, but it's used in the
// cast extension. This line prevents an exception on using localStorage.
window.__defineGetter__('localStorage', function() { return {}; });

/**
 * @type {string}
 * @const
 */
var CAST_COMMAND_LINE_FLAG = 'enable-video-player-chromecast-support';

// THIS IS A TEST APP.
// TODO(yoshiki): Fix this before launch.
var APPLICATION_ID = '214CC863';

chrome.commandLinePrivate.hasSwitch(CAST_COMMAND_LINE_FLAG, function(result) {
  if (!result)
    return;

  ensureLoad(initializeApi);
});

/**
 * Executes the given callback after the cast extension is initialized.
 * @param {function} callback Callback (executed asynchronously).
 */
function ensureLoad(callback) {
  if(!chrome.cast || !chrome.cast.isAvailable) {
    var checkTimer = setTimeout(function() {
      console.error('Either "Google Cast API" or "Google Cast" extension ' +
                    'seems not to be installed?');
    }, 5000);

    window['__onGCastApiAvailable'] = function(loaded, errorInfo) {
      if (loaded) {
        callback();
        clearTimeout(checkTimer);
      } else {
        console.error(errorInfo);
      }
    }
  } else {
    setTimeout(callback);  // Runs asynchronously.
  }
}

/**
 * Initialize Cast API.
 */
function initializeApi() {
  var onSession = function() {
    // TODO(yoshiki): Implement this.
  };

  var onInitSuccess = function() {
    // TODO(yoshiki): Implement this.
  };

  /**
   * @param {chrome.cast.Error} error
   */
  var onError = function(error) {
    console.error('Error on Cast initialization.', error);
  };

  var sessionRequest = new chrome.cast.SessionRequest(APPLICATION_ID);
  var apiConfig = new chrome.cast.ApiConfig(sessionRequest,
                                            onSession,
                                            onReceiver);
  chrome.cast.initialize(apiConfig, onInitSuccess, onError);
}

/**
 * @param {chrome.cast.ReceiverAvailability} availability Availability of casts.
 * @param {Array.<Object>} receivers List of casts.
 */
function onReceiver(availability, receivers) {
  if (availability === chrome.cast.ReceiverAvailability.AVAILABLE) {
    if (!receivers) {
      console.error('Receiver list is empty.');
      receivers = [];
    }

    player.setCastList(receivers);
  } else if (availability == chrome.cast.ReceiverAvailability.UNAVAILABLE) {
    player.setCastList([]);
  } else {
    console.error('Unexpected response in onReceiver.', arguments);
    player.setCastList([]);
  }
}
