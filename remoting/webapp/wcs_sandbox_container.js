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
  this.sandbox_ = sandbox;
  /** @type {?function(string):void} */
  this.onReady_ = null;
  /** @type {?function(remoting.Error):void} */
  this.onError_ = null;
  /** @type {?function(string):void} */
  this.onIq_ = null;
  /** @type {Object.<number, XMLHttpRequest>} */
  this.pendingXhrs_ = {};

  window.addEventListener('message', this.onMessage_.bind(this), false);
};

/**
 * @param {?function(string):void} onReady Callback invoked with the client JID
 *     when the WCS code has loaded.
 * @return {void} Nothing.
 */
remoting.WcsSandboxContainer.prototype.setOnReady = function(onReady) {
  this.onReady_ = onReady;
};

/**
 * @param {?function(remoting.Error):void} onError Callback invoked if the WCS
 *     code cannot be loaded.
 * @return {void} Nothing.
 */
remoting.WcsSandboxContainer.prototype.setOnError = function(onError) {
  this.onError_ = onError;
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
 * @param {string} token The access token.
 * @return {void}
 */
remoting.WcsSandboxContainer.prototype.setAccessToken = function(token) {
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

    case 'onReady':
      /** @type {string} */
      var clientJid = event.data['clientJid'];
      if (clientJid === undefined) {
        console.error('onReady: missing client JID');
        break;
      }
      if (this.onReady_) {
        this.onReady_(clientJid);
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