/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @fileoverview
 * A class that loads a WCS IQ client and constructs remoting.wcs as a
 * wrapper for it.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @type {remoting.WcsLoader} */
remoting.wcsLoader = null;

/**
 * @constructor
 */
remoting.WcsLoader = function() {
  /**
   * An OAuth2 access token.
   * @type {string}
   * @private
   */
  this.token_ = '';

  /**
   * A callback that gets an updated access token asynchronously.
   * @param {function(string): void} setToken The function to call when the
   *     token is available.
   * @private
   */
  this.tokenRefresh_ = function(setToken) {};

  /**
   * The function called when WCS is ready, or on error.
   * @param {boolean} success Whether or not the load succeeded.
   * @private
   */
  this.onReady_ = function(success) {};

  /**
   * @enum {string}
   * @private
   */
  this.LoadState_ = {
    NOT_STARTED: 'NOT_STARTED',
    STARTED: 'STARTED',
    READY: 'READY'
  };

  /**
   * The state of WCS loading.
   * @type {string}
   * @private
   */
  this.loadState_ = this.LoadState_.NOT_STARTED;

  /**
   * The WCS client that will be downloaded.
   * @type {remoting.WcsIqClient}
   */
  this.wcsIqClient = null;
};

/**
 * The URL of the GTalk gadget.
 * @type {string}
 * @private
 */
remoting.WcsLoader.prototype.TALK_GADGET_URL_ =
    'https://talkgadget.google.com/talkgadget/';

/**
 * Starts loading the WCS IQ client aynchronously. If an OAuth2 token is
 * already available, this is equivalent to the start method; if not, then
 * the client will be loaded after an OAuth token exchange has occurred.
 *
 * @param {function(function(string): void): void} tokenRefresh
 *     Gets a (possibly updated) access token asynchronously.
 * @param {function(boolean): void} onReady If the WCS connection is not yet
 *     ready, then |onReady| will be called with a true parameter when it is
 *     ready, or with a false parameter on error.
 * @return {void} Nothing.
 */
remoting.WcsLoader.prototype.startAsync = function(tokenRefresh, onReady) {
  /** @type {remoting.WcsLoader} */
  var that = this;
  /** @param {string} token The OAuth2 access token. */
  var start = function(token) {
    that.start(token, tokenRefresh, onReady);
  };
  remoting.oauth2.callWithToken(start);
};

/**
 * Starts loading the WCS IQ client.
 *
 * When it's loaded, construct remoting.wcs as a wrapper for it.
 * When the WCS connection is ready, call |onReady|.
 *
 * No guarantee is made about what will happen if this function is called more
 * than once.
 *
 * @param {string} token An OAuth2 access token.
 * @param {function(function(string): void): void} tokenRefresh
 *     Gets a (possibly updated) access token asynchronously.
 * @param {function(boolean): void} onReady If the WCS connection is not yet
 *     ready, then |onReady| will be called with a true parameter when it is
 *     ready, or with a false parameter on error.
 * @return {void} Nothing.
 */
remoting.WcsLoader.prototype.start = function(token, tokenRefresh, onReady) {
  this.token_ = token;
  this.tokenRefresh_ = tokenRefresh;
  this.onReady_ = onReady;
  if (this.loadState_ == this.LoadState_.READY) {
    this.onReady_(true);
    return;
  }
  if (this.loadState_ == this.LoadState_.STARTED) {
    return;
  }
  this.loadState_ = this.LoadState_.STARTED;
  var node = document.createElement('script');
  node.src = this.TALK_GADGET_URL_ + 'iq?access_token=' + this.token_;
  node.type = 'text/javascript';
  /** @type {remoting.WcsLoader} */
  var that = this;
  node.onload = function() { that.constructWcs_(); };
  node.onerror = function() { that.onReady_(false); };
  document.body.insertBefore(node, document.body.firstChild);
  return;
};

/**
 * Constructs the remoting.wcs object.
 *
 * @return {void} Nothing.
 * @private
 */
remoting.WcsLoader.prototype.constructWcs_ = function() {
  /** @type {remoting.WcsLoader} */
  var that = this;
  /** @param {function(string): void} setToken The function to call when the
      token is available. */
  var tokenRefresh = function(setToken) { that.tokenRefresh_(setToken); };
  remoting.wcs = new remoting.Wcs(
      remoting.wcsLoader.wcsIqClient,
      this.token_,
      function() { that.onWcsReady_(); },
      tokenRefresh);
};

/**
 * Notifies this object that WCS is ready.
 *
 * @return {void} Nothing.
 * @private
 */
remoting.WcsLoader.prototype.onWcsReady_ = function() {
  this.loadState_ = this.LoadState_.READY;
  this.onReady_(true);
  this.onReady_ = function(success) {};
};
