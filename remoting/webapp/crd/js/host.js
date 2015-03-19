// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The deserialized form of the chromoting host as returned by Apiary.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * Note that the object has more fields than are detailed below--these
 * are just the ones that we refer to directly.
 *
 * TODO(kelvinp):Make fields private and expose them via getters.
 * @constructor
 */
remoting.Host = function() {
  /** @type {string} */
  this.hostName = '';
  /** @type {string} */
  this.hostId = '';
  /** @type {string} */
  this.status = '';
  /** @type {string} */
  this.jabberId = '';
  /** @type {string} */
  this.publicKey = '';
  /** @type {string} */
  this.hostVersion = '';
  /** @type {Array<string>} */
  this.tokenUrlPatterns = [];
  /** @type {string} */
  this.updatedTime = '';
  /** @type {string} */
  this.hostOfflineReason = '';
  /** @type {remoting.Host.Options} */
  this.options = new remoting.Host.Options(this.hostId);
};

/**
 * @constructor
 * @param {string} hostId
 * @struct
 */
remoting.Host.Options = function(hostId) {
  /** @private */
  this.hostId_ = hostId;
  /** @type {boolean} */
  this.shrinkToFit = true;
  /** @type {boolean} */
  this.resizeToClient = true;
  /** @type {string} */
  this.remapKeys = '';
  /** @type {number} */
  this.desktopScale = 1;
  /** @type {remoting.PairingInfo} */
  this.pairingInfo = {clientId: '', sharedSecret: ''};
};

remoting.Host.Options.prototype.save = function() {
  // TODO(kelvinp): Migrate pairingInfo to use this class as well and get rid of
  // remoting.HostSettings.
  remoting.HostSettings.save(this.hostId_, this);
};


/** @return {Promise} A promise that resolves when the settings are loaded. */
remoting.Host.Options.prototype.load = function() {
  var that = this;
  return base.Promise.as(remoting.HostSettings.load, [this.hostId_]).then(
    /**
     * @param {Object.<string|boolean|number>} options
     */
    function(options) {
      // Must be defaulted to true so that app-remoting can resize the host
      // upon launching.
      // TODO(kelvinp): Uses a separate host options for app-remoting that
      // hardcodes resizeToClient to true.
      that.resizeToClient =
          base.getBooleanAttr(options, 'resizeToClient', true);
      that.shrinkToFit = base.getBooleanAttr(options, 'shrinkToFit', true);
      that.desktopScale = base.getNumberAttr(options, 'desktopScale', 1);
      that.remapKeys = base.getStringAttr(options, 'remapKeys', '');
      that.pairingInfo =
          /** @type {remoting.PairingInfo} */ (
              base.getObjectAttr(options, 'pairingInfo', that.pairingInfo));
    });
};

/**
 * Determine whether a host needs to be manually updated. This is the case if
 * the host's major version number is more than 1 lower than that of the web-
 * app (a difference of 1 is tolerated due to the different update mechanisms)
 * and if the host is on-line (off-line hosts are not expected to auto-update).
 *
 * @param {remoting.Host} host The host information from the directory.
 * @param {string|number} webappVersion The version number of the web-app, in
 *     either dotted-decimal notation notation, or directly represented by the
 *     major version.
 * @return {boolean} True if the host is on-line but out-of-date.
 */
remoting.Host.needsUpdate = function(host, webappVersion) {
  if (host.status != 'ONLINE') {
    return false;
  }
  var hostMajorVersion = parseInt(host.hostVersion, 10);
  if (isNaN(hostMajorVersion)) {
    // Host versions 26 and higher include the version number in heartbeats,
    // so if it's missing then the host is at most version 25.
    hostMajorVersion = 25;
  }
  return (parseInt(webappVersion, 10) - hostMajorVersion) > 1;
};

})();
