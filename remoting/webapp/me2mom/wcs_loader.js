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

(function() {
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
   * @type {function(function(string): void): void}
   * @private
   */
  this.refreshToken_ = function(setToken) {};

  /**
   * The function called when WCS is ready.
   * @type {function(): void}
   * @private
   */
  this.onReady_ = function() {};

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
   * @type {Object}
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
 * Starts loading the WCS IQ client.
 *
 * When it's loaded, construct remoting.wcs as a wrapper for it.
 * When the WCS connection is ready, call |onReady|.
 *
 * No guarantee is made about what will happen if this function is called more
 * than once.
 *
 * @param {string} token An OAuth2 access token.
 * @param {function(function(string): void): void} refreshToken
 *     Gets a (possibly updated) access token asynchronously.
 * @param {function(): void} onReady If the WCS connection is not yet
 *     ready, then |onReady| will be called when it is ready.
 * @return {void} Nothing.
 */
remoting.WcsLoader.prototype.start = function(token, refreshToken, onReady) {
  this.token_ = token;
  this.refreshToken_ = refreshToken;
  this.onReady_ = onReady;
  if (this.loadState_ == this.LoadState_.READY) {
    this.onReady_();
    return;
  }
  if (this.loadState_ == this.LoadState_.STARTED) {
    return;
  }
  this.loadState_ = this.LoadState_.STARTED;
  var node = document.createElement('script');
  node.src = this.TALK_GADGET_URL_ + 'iq?access_token=' + this.token_;
  node.type = 'text/javascript';
  var that = this;
  node.onload = function() { that.constructWcs_(); };
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
  var that = this;
  remoting.wcs = new remoting.Wcs(
      remoting.wcsLoader.wcsIqClient,
      this.token_,
      function() { that.onWcsReady_(); },
      function(setToken) { that.refreshToken_(setToken); });
};

/**
 * Notifies this object that WCS is ready.
 *
 * @return {void} Nothing.
 * @private
 */
remoting.WcsLoader.prototype.onWcsReady_ = function() {
  this.loadState_ = this.LoadState_.READY;
  this.onReady_();
  this.onReady_ = function() {};
};

}());
