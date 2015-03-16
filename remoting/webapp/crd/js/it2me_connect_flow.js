// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

// Length of the various components of the access code in number of digits.
var SUPPORT_ID_LENGTH = 7;
var HOST_SECRET_LENGTH = 5;
var ACCESS_CODE_LENGTH = SUPPORT_ID_LENGTH + HOST_SECRET_LENGTH;

/**
 * @param {remoting.SessionConnector} sessionConnector
 * @constructor
 * @private
 */
remoting.It2MeConnectFlow = function(sessionConnector) {
  /** @private */
  this.sessionConnector_ = sessionConnector;
  /** @private */
  this.hostId_ = '';
  /** @private */
  this.passCode_ = '';
};

/**
 * Initiates an IT2Me connection.
 *
 * @param {remoting.SessionConnector} connector
 * @param {string} accessCode
 * @return {Promise} Promise that resolves when the connection is initiated.
 *     Rejects with remoting.Error on failure.
 */
remoting.It2MeConnectFlow.start = function(connector, accessCode) {
  var instance = new remoting.It2MeConnectFlow(connector);
  return instance.connect_(accessCode);
};

/**
 * @param {string} accessCode The access code as entered by the user.
 * @return {Promise} Promise that resolves when the connection is initiated.
 *     Rejects with remoting.Error on failure.
 * @private
 */
remoting.It2MeConnectFlow.prototype.connect_ = function(accessCode) {
  var that = this;

  return this.verifyAccessCode_(accessCode).then(function() {
    return remoting.identity.getToken();
  }).then(function(/** string */ token) {
    return that.getHostInfo_(token);
  }).then(function(/** XMLHttpRequest */ xhr) {
    return that.onHostInfo_(xhr);
  }).then(function(/** remoting.Host */ host) {
    that.sessionConnector_.connect(
        remoting.DesktopConnectedView.Mode.IT2ME,
        host,
        new remoting.CredentialsProvider({ accessCode: that.passCode_ }));
  }).catch(function(error) {
    return Promise.reject(/** remoting.Error */ error);
  });
};

/**
 * @param {string} accessCode
 * @return {Promise}  Promise that resolves if the access code is valid.
 * @private
 */
remoting.It2MeConnectFlow.prototype.verifyAccessCode_ = function(accessCode) {
  var normalizedAccessCode = accessCode.replace(/\s/g, '');
  if (normalizedAccessCode.length !== ACCESS_CODE_LENGTH) {
    return Promise.reject(new remoting.Error(
        remoting.Error.Tag.INVALID_ACCESS_CODE));
  }

  this.hostId_ = normalizedAccessCode.substring(0, SUPPORT_ID_LENGTH);
  this.passCode_ = normalizedAccessCode;

  return Promise.resolve();
};

/**
 * Continues an IT2Me connection once an access token has been obtained.
 *
 * @param {string} token An OAuth2 access token.
 * @return {Promise<XMLHttpRequest>}
 * @private
 */
remoting.It2MeConnectFlow.prototype.getHostInfo_ = function(token) {
  var that = this;
  return new Promise(function(resolve) {
    remoting.xhr.start({
      method: 'GET',
      url: remoting.settings.DIRECTORY_API_BASE_URL + '/support-hosts/' +
           encodeURIComponent(that.hostId_),
      onDone: resolve,
      oauthToken: token
    });
  });
};

/**
 * Continues an IT2Me connection once the host JID has been looked up.
 *
 * @param {XMLHttpRequest} xhr The server response to the support-hosts query.
 * @return {!Promise<!remoting.Host>} Rejects on error.
 * @private
 */
remoting.It2MeConnectFlow.prototype.onHostInfo_ = function(xhr) {
  if (xhr.status == 200) {
    var response = /** @type {{data: {jabberId: string, publicKey: string}}} */
        (base.jsonParseSafe(xhr.responseText));
    if (response && response.data &&
        response.data.jabberId && response.data.publicKey) {
      var host = new remoting.Host();
      host.hostId = this.hostId_;
      host.jabberId = response.data.jabberId;
      host.publicKey = response.data.publicKey;
      host.hostName = response.data.jabberId.split('/')[0];
      return Promise.resolve(host);
    } else {
      console.error('Invalid "support-hosts" response from server.');
      return Promise.reject(remoting.Error.unexpected());
    }
  } else {
    return Promise.reject(translateSupportHostsError(xhr.status));
  }
};

/**
 * @param {number} error An HTTP error code returned by the support-hosts
 *     endpoint.
 * @return {remoting.Error} The equivalent remoting.Error code.
 */
function translateSupportHostsError(error) {
  switch (error) {
    case 0: return new remoting.Error(remoting.Error.Tag.NETWORK_FAILURE);
    case 404: return new remoting.Error(remoting.Error.Tag.INVALID_ACCESS_CODE);
    case 502: // No break
    case 503: return new remoting.Error(remoting.Error.Tag.SERVICE_UNAVAILABLE);
    default: return remoting.Error.unexpected();
  }
}

})();
