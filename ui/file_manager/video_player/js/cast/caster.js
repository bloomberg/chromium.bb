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

  // TODO(yoshiki): Check if the Google Cast extension is installed or not.
  // If not installed, we should skip all cast-related functionality.

  loadCastAPI(initializeApi);
});

/**
 * Executes the given callback after the cast extension is initialized.
 * @param {function} callback Callback (executed asynchronously).
 * @param {boolean=} opt_secondTry Spericy try if it's second call after
 *     installation of Cast API extension.
 */
function loadCastAPI(callback, opt_secondTry) {
  var script = document.createElement('script');

  var onError = function() {
    script.removeEventListener('error', onError);
    document.body.removeChild(script);

    if (opt_secondTry) {
      // Shows error message and exits if it's the 2nd try.
      console.error('Google Cast API extension load failed.');
      return;
    }

    // Installs the Google Cast API extension and retry loading.
    chrome.fileBrowserPrivate.installWebstoreItem(
        'mafeflapfdfljijmlienjedomfjfmhpd',
        true,  // Don't use installation prompt.
        function() {
          if (chrome.runtime.lastError) {
            console.error('Google Cast API extension installation error.',
                          chrome.runtime.lastError.message);
            return;
          }

          console.info('Google Cast API extension installed.');
          // Loads API again.
          setTimeout(loadCastAPI.bind(null, callback, true));
        }.wrap());
  }.wrap();

  var onLoad = function() {
    if(!chrome.cast || !chrome.cast.isAvailable) {
      var checkTimer = setTimeout(function() {
        console.error('Either "Google Cast API" or "Google Cast" extension ' +
                      'seems not to be installed?');
      }.wrap(), 5000);

      window['__onGCastApiAvailable'] = function(loaded, errorInfo) {
        clearTimeout(checkTimer);

        if (loaded)
          callback();
        else
          console.error('Google Cast exntnsion load failed.', errorInfo);
      }.wrap();
    } else {
      setTimeout(callback);  // Runs asynchronously.
    }
  }.wrap();

  script.src = '_modules/mafeflapfdfljijmlienjedomfjfmhpd/cast_sender.js';
  script.addEventListener('error', onError);
  script.addEventListener('load', onLoad);
  document.body.appendChild(script);
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
