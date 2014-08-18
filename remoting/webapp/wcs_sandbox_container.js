/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @fileoverview
 * The application side of the application/sandbox WCS interface, used by the
 * application to exchange messages with the sandbox.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {Window} sandbox The Javascript Window object representing the
 *     sandboxed WCS driver.
 * @constructor
 */
remoting.WcsSandboxContainer = function(sandbox) {
  /** @private */
  this.sandbox_ = sandbox;
  /** @type {?function(string):void}
    * @private */
  this.onConnected_ = null;
  /** @type {function(remoting.Error):void}
    * @private */
  this.onError_ = function(error) {};
  /** @type {?function(string):void}
    * @private */
  this.onIq_ = null;
  /** @type {Object.<number, XMLHttpRequest>}
    * @private */
  this.pendingXhrs_ = {};
  /** @private */
  this.localJid_ = '';

  /** @private */
  this.accessTokenRefreshTimerStarted_ = false;

  window.addEventListener('message', this.onMessage_.bind(this), false);

  if (base.isAppsV2()) {
    var message = {
      'command': 'proxyXhrs'
    };
    this.sandbox_.postMessage(message, '*');
  }
};

/**
 * @param {function(string):void} onConnected Callback to be called when WCS is
 *     connected. May be called synchronously if WCS is already connected.
 * @param {function(remoting.Error):void} onError called in case of an error.
 * @return {void} Nothing.
 */
remoting.WcsSandboxContainer.prototype.connect = function(
    onConnected, onError) {
  this.onError_ = onError;
  this.ensureAccessTokenRefreshTimer_();
  if (this.localJid_) {
    onConnected(this.localJid_);
  } else {
    this.onConnected_ = onConnected;
  }
};

/**
 * @param {?function(string):void} onIq Callback invoked when an IQ stanza is
 *     received.
 * @return {void} Nothing.
 */
remoting.WcsSandboxContainer.prototype.setOnIq = function(onIq) {
  this.onIq_ = onIq;
};

/**
 * Refreshes access token and starts a timer to update it periodically.
 *
 * @private
 */
remoting.WcsSandboxContainer.prototype.ensureAccessTokenRefreshTimer_ =
    function() {
  if (this.accessTokenRefreshTimerStarted_) {
    return;
  }

  this.refreshAccessToken_();
  setInterval(this.refreshAccessToken_.bind(this), 60 * 1000);
  this.accessTokenRefreshTimerStarted_ = true;
}

/**
 * @private
 * @return {void} Nothing.
 */
remoting.WcsSandboxContainer.prototype.refreshAccessToken_ = function() {
  remoting.identity.callWithToken(
      this.setAccessToken_.bind(this), this.onError_);
};

/**
 * @private
 * @param {string} token The access token.
 * @return {void}
 */
remoting.WcsSandboxContainer.prototype.setAccessToken_ = function(token) {
  var message = {
    'command': 'setAccessToken',
    'token': token
  };
  this.sandbox_.postMessage(message, '*');
};

/**
 * @param {string} stanza The IQ stanza to send.
 * @return {void}
 */
remoting.WcsSandboxContainer.prototype.sendIq = function(stanza) {
  var message = {
    'command': 'sendIq',
    'stanza': stanza
  };
  this.sandbox_.postMessage(message, '*');
};

/**
 * Event handler to process messages from the sandbox.
 *
 * @param {Event} event
 */
remoting.WcsSandboxContainer.prototype.onMessage_ = function(event) {
  switch (event.data['command']) {

    case 'onLocalJid':
      /** @type {string} */
      var localJid = event.data['localJid'];
      if (localJid === undefined) {
        console.error('onReady: missing localJid');
        break;
      }
      this.localJid_ = localJid;
      if (this.onConnected_) {
        var callback = this.onConnected_;
        this.onConnected_ = null;
        callback(localJid);
      }
      break;

    case 'onError':
      /** @type {remoting.Error} */
      var error = event.data['error'];
      if (error === undefined) {
        console.error('onError: missing error code');
        break;
      }
      this.onError_(error);
      break;

    case 'onIq':
      /** @type {string} */
      var stanza = event.data['stanza'];
      if (stanza === undefined) {
        console.error('onIq: missing IQ stanza');
        break;
      }
      if (this.onIq_) {
        this.onIq_(stanza);
      }
      break;

    case 'sendXhr':
      /** @type {number} */
      var id = event.data['id'];
      if (id === undefined) {
        console.error('sendXhr: missing id');
        break;
      }
      /** @type {Object} */
      var parameters = event.data['parameters'];
      if (parameters === undefined) {
        console.error('sendXhr: missing parameters');
        break;
      }
      /** @type {string} */
      var method = parameters['method'];
      if (method === undefined) {
        console.error('sendXhr: missing method');
        break;
      }
      /** @type {string} */
      var url = parameters['url'];
      if (url === undefined) {
        console.error('sendXhr: missing url');
        break;
      }
      /** @type {string} */
      var data = parameters['data'];
      if (data === undefined) {
        console.error('sendXhr: missing data');
        break;
      }
      /** @type {string|undefined}*/
      var user = parameters['user'];
      /** @type {string|undefined}*/
      var password = parameters['password'];
      var xhr = new XMLHttpRequest;
      this.pendingXhrs_[id] = xhr;
      xhr.open(method, url, true, user, password);
      /** @type {Object} */
      var headers = parameters['headers'];
      if (headers) {
        for (var header in headers) {
          xhr.setRequestHeader(header, headers[header]);
        }
      }
      xhr.onreadystatechange = this.onReadyStateChange_.bind(this, id);
      xhr.send(data);
      break;

    case 'abortXhr':
      var id = event.data['id'];
      if (id === undefined) {
        console.error('abortXhr: missing id');
        break;
      }
      var xhr = this.pendingXhrs_[id]
      if (!xhr) {
        // It's possible for an abort and a reply to cross each other on the
        // IPC channel. In that case, we silently ignore the abort.
        break;
      }
      xhr.abort();
      break;

    default:
      console.error('Unexpected message:', event.data['command'], event.data);
  }
};

/**
 * Return a "copy" of an XHR object suitable for postMessage. Specifically,
 * remove all non-serializable members such as functions.
 *
 * @param {XMLHttpRequest} xhr The XHR to serialize.
 * @return {Object} A serializable version of the input.
 */
function sanitizeXhr_(xhr) {
  /** @type {Object} */
  var result = {
    readyState: xhr.readyState,
    response: xhr.response,
    responseText: xhr.responseText,
    responseType: xhr.responseType,
    responseXML: xhr.responseXML,
    status: xhr.status,
    statusText: xhr.statusText,
    withCredentials: xhr.withCredentials
  };
  return result;
}

/**
 * @param {number} id The unique ID of the XHR for which the state has changed.
 * @private
 */
remoting.WcsSandboxContainer.prototype.onReadyStateChange_ = function(id) {
  var xhr = this.pendingXhrs_[id];
  if (!xhr) {
    // XHRs are only removed when they have completed, in which case no
    // further callbacks should be received.
    console.error('Unexpected callback for xhr', id);
    return;
  }
  var message = {
    'command': 'xhrStateChange',
    'id': id,
    'xhr': sanitizeXhr_(xhr)
  };
  this.sandbox_.postMessage(message, '*');
  if (xhr.readyState == 4) {
    delete this.pendingXhrs_[id];
  }
}

/** @type {remoting.WcsSandboxContainer} */
remoting.wcsSandbox = null;
