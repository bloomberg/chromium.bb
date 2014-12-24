// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This hack prevents a bug on the cast extension.
// TODO(yoshiki): Remove this once the cast extension supports Chrome apps.
// Although localStorage in Chrome app is not supported, but it's used in the
// cast extension. This line prevents an exception on using localStorage.
window.__defineGetter__('localStorage', function() { return {}; });

/**
 * @type {string}
 * @const
 */
var APPLICATION_ID = '4CCB98DA';

util.addPageLoadHandler(function() {
  initialize();
}.wrap());

/**
 * Starts initialization of cast-related feature.
 */
function initialize() {
  if (window.loadMockCastExtensionForTest) {
    // If the test flag is set, the mock extension for test will be laoded by
    // the test script. Sets the handler to wait for loading.
    onLoadCastExtension(initializeApi);
    return;
  }

  CastExtensionDiscoverer.findInstalledExtension(function(foundId) {
    if (foundId) {
      loadCastAPI(initializeApi);
    } else {
      console.info('No Google Cast extension is installed.');

      metrics.recordCastAPIExtensionStatus(
          metrics.CAST_API_EXTENSION_STATUS.SKIPPED);
    }
  }.wrap());
}

/**
 * Loads the cast API extention. If not install, the extension is installed
 * in background before load. The cast API will load the cast SDK automatically.
 * The given callback is executes after the cast SDK extension is initialized.
 *
 * @param {function()} callback Callback (executed asynchronously).
 * @param {boolean=} opt_secondTry Specify true if it's the second call after
 *     installation of Cast API extension.
 */
function loadCastAPI(callback, opt_secondTry) {
  var script = document.createElement('script');

  var onError = function() {
    script.removeEventListener('error', onError);
    document.body.removeChild(script);

    if (opt_secondTry) {
      metrics.recordCastAPIExtensionStatus(
          metrics.CAST_API_EXTENSION_STATUS.LOAD_FAILED);

      // Shows error message and exits if it's the 2nd try.
      console.error('Google Cast API extension load failed.');
      return;
    }

    // Installs the Google Cast API extension and retry loading.
    chrome.fileManagerPrivate.installWebstoreItem(
        'mafeflapfdfljijmlienjedomfjfmhpd',
        true,  // Don't use installation prompt.
        function() {
          if (chrome.runtime.lastError) {
            metrics.recordCastAPIExtensionStatus(
                metrics.CAST_API_EXTENSION_STATUS.INSTALLATION_FAILED);

            console.error('Google Cast API extension installation error.',
                          chrome.runtime.lastError.message);
            return;
          }

          console.info('Google Cast API extension installed.');

          // Loads API again.
          setTimeout(loadCastAPI.bind(null, callback, true), 0);
        }.wrap());
  }.wrap();

  // Trys to load the cast API extention which is defined in manifest.json.
  script.src = '_modules/mafeflapfdfljijmlienjedomfjfmhpd/cast_sender.js';
  script.addEventListener('error', onError);
  script.addEventListener(
      'load', onLoadCastExtension.bind(null, callback, opt_secondTry));
  document.body.appendChild(script);
}

/**
 * Loads the cast sdk extension.
 * @param {function()} callback Callback (executed asynchronously).
 * @param {boolean=} opt_installationOccured True if the extension is just
 *     installed in this window. False or null if it's already installed.
 */
function onLoadCastExtension(callback, opt_installationOccured) {
  var executeCallback = function() {
    if (opt_installationOccured) {
      metrics.recordCastAPIExtensionStatus(
          metrics.CAST_API_EXTENSION_STATUS.INSTALLED_AND_LOADED);
    } else {
      metrics.recordCastAPIExtensionStatus(
          metrics.CAST_API_EXTENSION_STATUS.LOADED);
    }

    setTimeout(callback, 0);  // Runs asynchronously.
  };

  if(!chrome.cast || !chrome.cast.isAvailable) {
    var checkTimer = setTimeout(function() {
      console.error('Either "Google Cast API" or "Google Cast" extension ' +
                    'seems not to be installed?');

      metrics.recordCastAPIExtensionStatus(
          metrics.CAST_API_EXTENSION_STATUS.LOAD_FAILED);
    }.wrap(), 5000);

    window['__onGCastApiAvailable'] = function(loaded, errorInfo) {
      clearTimeout(checkTimer);

      if (loaded) {
        executeCallback();
      } else {
        metrics.recordCastAPIExtensionStatus(
            metrics.CAST_API_EXTENSION_STATUS.LOAD_FAILED);

        console.error('Google Cast extension load failed.', errorInfo);
      }
    }.wrap();
  } else {
    // Just executes the callback since the API is already loaded.
    executeCallback();
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
 * Called when receiver availability is changed. This method is also called when
 * initialization is completed.
 *
 * @param {chrome.cast.ReceiverAvailability} availability Availability of casts.
 * @param {Array.<Object>} receivers List of casts.
 */
function onReceiver(availability, receivers) {
  if (availability === chrome.cast.ReceiverAvailability.AVAILABLE) {
    if (!receivers) {
      console.error('Receiver list is empty.');
      receivers = [];
    }

    metrics.recordNumberOfCastDevices(receivers.length);
    player.setCastList(receivers);
  } else if (availability == chrome.cast.ReceiverAvailability.UNAVAILABLE) {
    metrics.recordNumberOfCastDevices(0);
    player.setCastList([]);
  } else {
    console.error('Unexpected response in onReceiver.', arguments);
    player.setCastList([]);
  }
}
