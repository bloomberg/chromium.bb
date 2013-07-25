/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @fileoverview
 * The sandbox isn't allowed to make XHRs, so they have to be proxied to the
 * main process. The XMLHttpRequestProxy class is API-compatible with the
 * XMLHttpRequest class, but forwards the requests to the main process where
 * they can be serviced. The forwarding of XHRs and responses is handled by
 * the WcsSandboxContent class; this class is just a thin wrapper to hook XHR
 * creations by JS running in the sandbox.
 *
 * Because XMLHttpRequest is implemented natively, and because the intent is
 * to replace its functionality entirely, prototype linking is not a suitable
 * approach here, so much of the interface definition is duplicated from the
 * w3c specification: http://www.w3.org/TR/XMLHttpRequest
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @extends {XMLHttpRequest}
 */
remoting.XMLHttpRequestProxy = function() {
  /**
   * @type {{headers: Object}}
   */
  this.sandbox_ipc = {
    headers: {}
  };
  /**
   * @type {number}
   * @private
   */
  this.xhr_id_ = -1;
};

remoting.XMLHttpRequestProxy.prototype.open = function(
    method, url, async, user, password) {
  if (!async) {
    console.warn('Synchronous XHRs are not supported.');
  }
  this.sandbox_ipc.method = method;
  this.sandbox_ipc.url = url.toString();
  this.sandbox_ipc.user = user;
  this.sandbox_ipc.password = password;
};

remoting.XMLHttpRequestProxy.prototype.send = function(data) {
  if (remoting.sandboxContent) {
    this.sandbox_ipc.data = data;
    this.xhr_id_ = remoting.sandboxContent.sendXhr(this);
  }
};

remoting.XMLHttpRequestProxy.prototype.setRequestHeader = function(
    header, value) {
  this.sandbox_ipc.headers[header] = value;
};

remoting.XMLHttpRequestProxy.prototype.abort = function() {
  if (this.xhr_id_ != -1) {
    remoting.sandboxContent.abortXhr(this.xhr_id_);
  }
};

remoting.XMLHttpRequestProxy.prototype.getResponseHeader = function(header) {
  console.error('Sandbox: unproxied getResponseHeader(' + header + ') called.');
};

remoting.XMLHttpRequestProxy.prototype.getAllResponseHeaders = function() {
  console.error('Sandbox: unproxied getAllResponseHeaders called.');
};

remoting.XMLHttpRequestProxy.prototype.overrideMimeType = function() {
  console.error('Sandbox: unproxied overrideMimeType called.');
};

remoting.XMLHttpRequestProxy.prototype.UNSENT = 0;
remoting.XMLHttpRequestProxy.prototype.OPENED = 1;
remoting.XMLHttpRequestProxy.prototype.HEADERS_RECEIVED = 2;
remoting.XMLHttpRequestProxy.prototype.LOADING = 3;
remoting.XMLHttpRequestProxy.prototype.DONE = 4;
