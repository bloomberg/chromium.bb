// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A mock version of HostDaemonFacade.  Internally all
 * delays are implemented with promises, so SpyPromise can be used to
 * wait out delays.
 *
 * By default, every method fails.  Methods can be individually set to
 * pass specific values to their onDone arguments by setting member
 * variables of the mock object.
 *
 * When methods fail, they set the detail field of the remoting.Error
 * object to the name of the method that failed.
 */

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * By default, all methods fail.
 * @constructor
 */
remoting.MockHostDaemonFacade = function() {
  /** @type {Array<remoting.HostController.Feature>} */
  this.features = [];

  /** @type {?string} */
  this.hostName = null;

  /** @type {?string} */
  this.pinHash = null;

  /** @type {?string} */
  this.privateKey = null;

  /** @type {?string} */
  this.publicKey = null;

  /** @type {Object} */
  this.daemonConfig = null;

  /** @type {?string} */
  this.daemonVersion = null;

  /** @type {?boolean} */
  this.consentSupported = null;

  /** @type {?boolean} */
  this.consentAllowed = null;

  /** @type {?boolean} */
  this.consentSetByPolicy = null;

  /** @type {?remoting.HostController.AsyncResult} */
  this.startDaemonResult = null;

  /** @type {?remoting.HostController.AsyncResult} */
  this.stopDaemonResult = null;

  /** @type {?remoting.HostController.State} */
  this.daemonState = null;

  /** @type {Array<remoting.PairedClient>} */
  this.pairedClients = null;

  /** @type {?string} */
  this.hostClientId = null;

  /** @type {?string} */
  this.userEmail = null;

  /** @type {?string} */
  this.refreshToken = null;
};

/**
 * @param {remoting.HostController.Feature} feature
 * @param {function(boolean):void} onDone
 * @return {boolean}
 */
remoting.MockHostDaemonFacade.prototype.hasFeature = function(feature, onDone) {
  var that = this;
  Promise.resolve().then(function() {
    onDone(that.features.indexOf(feature) >= 0);
  });
};

/**
 * @param {function(string):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.getHostName =
    function(onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.hostName === null) {
      onError(remoting.Error.unexpected('getHostName'));
    } else {
      onDone(that.hostName);
    }
  });
};

/**
 * @param {string} hostId
 * @param {string} pin
 * @param {function(string):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.getPinHash =
    function(hostId, pin, onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.pinHash === null) {
      onError(remoting.Error.unexpected('getPinHash'));
    } else {
      onDone(that.pinHash);
    }
  });
};

/**
 * @param {function(string, string):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.generateKeyPair =
    function(onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.privateKey === null || that.publicKey === null) {
      onError(remoting.Error.unexpected('generateKeyPair'));
    } else {
      onDone(that.privateKey, that.publicKey);
    }
  });
};

/**
 * @param {Object} config
 * @param {function(remoting.HostController.AsyncResult):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.updateDaemonConfig =
    function(config, onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.daemonConfig === null ||
        'host_id' in config ||
        'xmpp_login' in config) {
      onError(remoting.Error.unexpected('updateDaemonConfig'));
    } else {
      base.mix(that.daemonConfig, config);
      onDone(remoting.HostController.AsyncResult.OK);
    }
  });
};

/**
 * @param {function(Object):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.getDaemonConfig =
    function(onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.daemonConfig === null) {
      onError(remoting.Error.unexpected('getDaemonConfig'));
    } else {
      onDone(that.daemonConfig);
    }
  });
};

/**
 * @param {function(string):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.getDaemonVersion =
    function(onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.daemonVersion === null) {
      onError(remoting.Error.unexpected('getDaemonVersion'));
    } else {
      onDone(that.daemonVersion);
    }
  });
};

/**
 * @param {function(boolean, boolean, boolean):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.getUsageStatsConsent =
    function(onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.consentSupported === null ||
        that.consentAllowed === null ||
        that.consentSetByPolicy === null) {
      onError(remoting.Error.unexpected('getUsageStatsConsent'));
    } else {
      onDone(
          that.consentSupported,
          that.consentAllowed,
          that.consentSetByPolicy);
    }
  });
};

/**
 * @param {Object} config
 * @param {boolean} consent Consent to report crash dumps.
 * @param {function(remoting.HostController.AsyncResult):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.startDaemon =
    function(config, consent, onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.startDaemonResult === null) {
      onError(remoting.Error.unexpected('startDaemon'));
    } else {
      onDone(that.startDaemonResult);
    }
  });
};

/**
 * @param {function(remoting.HostController.AsyncResult):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.stopDaemon =
    function(onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.stopDaemonResult === null) {
      onError(remoting.Error.unexpected('stopDaemon'));
    } else {
      onDone(that.stopDaemonResult);
    }
  });
};

/**
 * @param {function(remoting.HostController.State):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.getDaemonState =
    function(onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.daemonState === null) {
      onError(remoting.Error.unexpected('getDaemonState'));
    } else {
      onDone(that.daemonState);
    }
  });
};

/**
 * @param {function(Array<remoting.PairedClient>):void} onDone
 * @param {function(!remoting.Error):void} onError
 */
remoting.MockHostDaemonFacade.prototype.getPairedClients =
    function(onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.pairedClients === null) {
      onError(remoting.Error.unexpected('getPairedClients'));
    } else {
      onDone(that.pairedClients);
    }
  });
};

/**
 * @param {function(boolean):void} onDone
 * @param {function(!remoting.Error):void} onError
 */
remoting.MockHostDaemonFacade.prototype.clearPairedClients =
    function(onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.pairedClients === null) {
      onError(remoting.Error.unexpected('clearPairedClients'));
    } else {
      that.pairedClients = [];
      onDone(true);
    }
  });
};

/**
 * @param {string} client
 * @param {function(boolean):void} onDone
 * @param {function(!remoting.Error):void} onError
 */
remoting.MockHostDaemonFacade.prototype.deletePairedClient =
    function(client, onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.pairedClients === null) {
      onError(remoting.Error.unexpected('deletePairedClient'));
    } else {
      that.pairedClients = that.pairedClients.filter(function(/** Object */ c) {
        return c['clientId'] != client;
      });
      onDone(true);
    }
  });
};

/**
 * @param {function(string):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.getHostClientId =
    function(onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.hostClientId === null) {
      onError(remoting.Error.unexpected('getHostClientId'));
    } else {
      onDone(that.hostClientId);
    }
  });
};

/**
 * @param {string} authorizationCode
 * @param {function(string, string):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {void}
 */
remoting.MockHostDaemonFacade.prototype.getCredentialsFromAuthCode =
    function(authorizationCode, onDone, onError) {
  var that = this;
  Promise.resolve().then(function() {
    if (that.userEmail === null || that.refreshToken === null) {
      onError(remoting.Error.unexpected('getCredentialsFromAuthCode'));
    } else {
      onDone(that.userEmail, that.refreshToken);
    }
  });
};

})();
