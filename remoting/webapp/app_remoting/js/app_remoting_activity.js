// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Type definition for the RunApplicationResponse returned by the API.
 * @typedef {{
 *   status: string,
 *   hostJid: string,
 *   authorizationCode: string,
 *   sharedSecret: string,
 *   host: {
 *     applicationId: string,
 *     hostId: string
 *   }
 * }}
 */
remoting.AppHostResponse;

(function() {

'use strict';

/**
 * @param {Array<string>} appCapabilities Array of application capabilities.
 * @param {remoting.Application} app
 * @param {remoting.WindowShape} windowShape
 * @param {string} subscriptionToken
 * @param {base.WindowMessageDispatcher} windowMessageDispatcher
 *
 * @constructor
 * @implements {remoting.Activity}
 */
remoting.AppRemotingActivity = function(appCapabilities, app, windowShape,
                                        subscriptionToken,
                                        windowMessageDispatcher) {
  /** @private */
  this.sessionFactory_ = new remoting.ClientSessionFactory(
      document.querySelector('#client-container .client-plugin-container'),
      appCapabilities);

  /** @private {remoting.ClientSession} */
  this.session_ = null;

  /** @private {base.Disposables} */
  this.connectedDisposables_ = null;

  /** @private */
  this.app_ = app;

  /** @private */
  this.windowShape_ = windowShape;

  /** @private */
  this.subscriptionToken_ = subscriptionToken;

  /** @private {base.WindowMessageDispatcher} */
  this.windowMessageDispatcher_ = windowMessageDispatcher;
};

remoting.AppRemotingActivity.prototype.dispose = function() {
  this.cleanup_();
  remoting.LoadingWindow.close();
};

remoting.AppRemotingActivity.prototype.start = function() {
  remoting.LoadingWindow.show();
  var that = this;
  return remoting.identity.getToken().then(function(/** string */ token) {
    return that.getAppHostInfo_(token);
  }).then(function(/** !remoting.Xhr.Response */ response) {
    that.onAppHostResponse_(response);
  }).catch(function(/** !remoting.Error */ error) {
    that.onConnectionFailed(error);
  });
};

remoting.AppRemotingActivity.prototype.stop = function() {
  if (this.session_) {
    this.session_.disconnect(remoting.Error.none());
  }
};

/** @private */
remoting.AppRemotingActivity.prototype.cleanup_ = function() {
  base.dispose(this.connectedDisposables_);
  this.connectedDisposables_ = null;
  base.dispose(this.session_);
  this.session_ = null;
};

/**
 * @param {string} token
 * @return {Promise<!remoting.Xhr.Response>}
 * @private
 */
remoting.AppRemotingActivity.prototype.getAppHostInfo_ = function(token) {
  var url = remoting.settings.APP_REMOTING_API_BASE_URL + '/applications/' +
            this.app_.getApplicationId() + '/run';
  // TODO(kelvinp): Passes |this.subscriptionToken_| to the XHR.
  return new remoting.AutoRetryXhr({
    method: 'POST',
    url: url,
    oauthToken: token
  }).start();
};

/**
 * @param {!remoting.Xhr.Response} xhrResponse
 * @private
 */
remoting.AppRemotingActivity.prototype.onAppHostResponse_ =
    function(xhrResponse) {
  if (xhrResponse.status == 200) {
    var response = /** @type {remoting.AppHostResponse} */
        (base.jsonParseSafe(xhrResponse.getText()));
    if (response &&
        response.status &&
        response.status == 'done' &&
        response.hostJid &&
        response.authorizationCode &&
        response.sharedSecret &&
        response.host &&
        response.host.hostId) {
      var hostJid = response.hostJid;
      var host = new remoting.Host(response.host.hostId);
      host.jabberId = hostJid;
      host.authorizationCode = response.authorizationCode;
      host.sharedSecret = response.sharedSecret;

      remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);

      /**
       * @param {string} tokenUrl Token-issue URL received from the host.
       * @param {string} hostPublicKey Host public key (DER and Base64
       *     encoded).
       * @param {string} scope OAuth scope to request the token for.
       * @param {function(string, string):void} onThirdPartyTokenFetched
       *     Callback.
       */
      var fetchThirdPartyToken = function(
          tokenUrl, hostPublicKey, scope, onThirdPartyTokenFetched) {
        // Use the authentication tokens returned by the app-remoting server.
        onThirdPartyTokenFetched(host['authorizationCode'],
                                 host['sharedSecret']);
      };

      var credentialsProvider = new remoting.CredentialsProvider(
              {fetchThirdPartyToken: fetchThirdPartyToken});
      var that = this;
      var logger = remoting.SessionLogger.createForClient();

      this.sessionFactory_.createSession(this, logger).then(
        function(/** remoting.ClientSession */ session) {
          that.session_ = session;
          logger.setLogEntryMode(remoting.ChromotingEvent.Mode.LGAPP);
          session.connect(host, credentialsProvider);
      });
    } else if (response && response.status == 'pending') {
      this.onConnectionFailed(new remoting.Error(
          remoting.Error.Tag.SERVICE_UNAVAILABLE));
    }
  } else {
    console.error('Invalid "runApplication" response from server.');
    // The orchestrator returns 403 if the user is not whitelisted to run the
    // app, which gets translated to a generic error message, so pick something
    // a bit more user-friendly.
    var error = xhrResponse.status == 403 ?
        new remoting.Error(remoting.Error.Tag.APP_NOT_AUTHORIZED) :
        remoting.Error.fromHttpStatus(xhrResponse.status);
    this.onConnectionFailed(error);
  }
};

/**
 * @param {remoting.ConnectionInfo} connectionInfo
 */
remoting.AppRemotingActivity.prototype.onConnected = function(connectionInfo) {
  var connectedView = new remoting.AppConnectedView(
      document.getElementById('client-container'),
      this.windowShape_, connectionInfo, this.windowMessageDispatcher_);

  var idleDetector = new remoting.IdleDetector(
      document.getElementById('idle-dialog'),
      this.windowShape_,
      this.app_.getApplicationName(),
      this.stop.bind(this));

  // Map Cmd to Ctrl on Mac since hosts typically use Ctrl for keyboard
  // shortcuts, but we want them to act as natively as possible.
  if (remoting.platformIsMac()) {
    connectionInfo.plugin().setRemapKeys({
      0x0700e3: 0x0700e0,
      0x0700e7: 0x0700e4
    });
  }

  // Drop the session after 30s of suspension as we cannot recover from a
  // connectivity loss longer than 30s anyways.
  this.session_.dropSessionOnSuspend(30 * 1000);
  this.connectedDisposables_ =
      new base.Disposables(idleDetector, connectedView);
};

/**
 * @param {remoting.Error} error
 */
remoting.AppRemotingActivity.prototype.onDisconnected = function(error) {
  if (error.isNone()) {
    this.app_.quit();
  } else {
    this.onConnectionDropped_();
  }
};

/**
 * @param {!remoting.Error} error
 */
remoting.AppRemotingActivity.prototype.onConnectionFailed = function(error) {
  remoting.LoadingWindow.close();
  this.showErrorMessage_(error);
  this.cleanup_();
};

/** @private */
remoting.AppRemotingActivity.prototype.onConnectionDropped_ = function() {
  // Don't dispose the session here to keep the plugin alive so that we can show
  // the last frame of the remote application window.
  base.dispose(this.connectedDisposables_);
  this.connectedDisposables_ = null;

  if (base.isOnline()) {
    this.reconnect_();
    return;
  }

  var rootElement = /** @type {HTMLDialogElement} */ (
      document.getElementById('connection-dropped-dialog'));
  var dialog =
      new remoting.ConnectionDroppedDialog(rootElement, this.windowShape_);
  var that = this;
  dialog.show().then(function(){
    dialog.dispose();
    that.reconnect_();
  }).catch(function(){
    dialog.dispose();
    that.app_.quit();
  });
};

/** @private */
remoting.AppRemotingActivity.prototype.reconnect_ = function() {
  // Hide the windows of the remote application with setDesktopRects([])
  // before tearing down the plugin.
  this.windowShape_.setDesktopRects([]);
  this.cleanup_();
  this.start();
};

/**
 * @param {!remoting.Error} error The error to be localized and displayed.
 * @private
 */
remoting.AppRemotingActivity.prototype.showErrorMessage_ = function(error) {
  console.error('Connection failed: ' + error.toString());
  remoting.MessageWindow.showErrorMessage(
      this.app_.getApplicationName(),
      chrome.i18n.getMessage(error.getTag()));
};

})();
