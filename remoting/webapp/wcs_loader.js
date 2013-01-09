/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
   * The WCS client that will be downloaded. This variable is initialized (via
   * remoting.wcsLoader) by the downloaded Javascript.
   * @type {remoting.WcsIqClient}
   */
  this.wcsIqClient = null;
};

/**
 * Load WCS if necessary, then invoke the callback with an access token.
 *
 * @param {function(string): void} onReady The callback function, called with
 *     an OAuth2 access token when WCS has been loaded.
 * @param {function(remoting.Error):void} onError Function to invoke with an
 *     error code on failure.
 * @return {void} Nothing.
 */
remoting.WcsLoader.load = function(onReady, onError) {
  if (!remoting.wcsLoader) {
    remoting.wcsLoader = new remoting.WcsLoader();
  }
  /** @param {string} token The OAuth2 access token. */
  var start = function(token) {
    remoting.wcsLoader.start_(token, onReady, onError);
  };
  remoting.identity.callWithToken(start, onError);
};

/**
 * The URL of the GTalk gadget.
 * @type {string}
 * @private
 */
remoting.WcsLoader.prototype.TALK_GADGET_URL_ =
    'https://chromoting-client.talkgadget.google.com/talkgadget/';

/**
 * The id of the script node.
 * @type {string}
 * @private
 */
remoting.WcsLoader.prototype.SCRIPT_NODE_ID_ = 'wcs-script-node';

/**
 * The attribute name indicating that the WCS has finished loading.
 * @type {string}
 * @private
 */
remoting.WcsLoader.prototype.SCRIPT_NODE_LOADED_FLAG_ = 'wcs-script-loaded';

/**
 * Starts loading the WCS IQ client.
 *
 * When it's loaded, construct remoting.wcs as a wrapper for it.
 * When the WCS connection is ready, or on error, call |onReady| or |onError|,
 * respectively.
 *
 * @param {string} token An OAuth2 access token.
 * @param {function(string): void} onReady The callback function, called with
 *     an OAuth2 access token when WCS has been loaded.
 * @param {function(remoting.Error):void} onError Function to invoke with an
 *     error code on failure.
 * @return {void} Nothing.
 * @private
 */
remoting.WcsLoader.prototype.start_ = function(token, onReady, onError) {
  var node = document.getElementById(this.SCRIPT_NODE_ID_);
  if (!node) {
    // The first time, there will be no script node, so create one.
    node = document.createElement('script');
    node.id = this.SCRIPT_NODE_ID_;
    node.src = this.TALK_GADGET_URL_ + 'iq?access_token=' + token;
    node.type = 'text/javascript';
    document.body.insertBefore(node, document.body.firstChild);
  } else if (node.hasAttribute(this.SCRIPT_NODE_LOADED_FLAG_)) {
    // Subsequently, explicitly invoke onReady if onload has already fired.
    // TODO(jamiewalch): It's possible that the WCS client has not finished
    // initializing. Add support for multiple callbacks to the remoting.Wcs
    // class to address this.
    onReady(token);
    return;
  }
  /** @type {remoting.WcsLoader} */
  var that = this;
  var onLoad = function() {
    var typedNode = /** @type {Element} */ (node);
    typedNode.setAttribute(that.SCRIPT_NODE_LOADED_FLAG_, true);
    that.constructWcs_(token, onReady);
  };
  var onLoadError = function(event) {
    // The DOM Event object has no detail on the nature of the error, so try to
    // validate the token to get a better idea.
    /** @param {remoting.Error} error Error code. */
    var onValidateError = function(error) {
      var typedNode = /** @type {Element} */ (node);
      typedNode.parentNode.removeChild(node);
      onError(error);
    };
    var onValidateOk = function() {
      // We can reach the authentication server and validate the token. Either
      // there's something wrong with the talkgadget service, or there is a
      // cookie problem. Only the cookie problem can be fixed by the user, so
      // suggest that fix.
      onValidateError(remoting.Error.AUTHENTICATION_FAILED);
    }
    remoting.oauth2.validateToken(token, onValidateOk, onValidateError);
  }
  node.addEventListener('load', onLoad, false);
  node.addEventListener('error', onLoadError, false);
};

/**
 * Constructs the remoting.wcs object.
 *
 * @param {string} token An OAuth2 access token.
 * @param {function(string): void} onReady The callback function, called with
 *     an OAuth2 access token when WCS has been loaded.
 * @return {void} Nothing.
 * @private
 */
remoting.WcsLoader.prototype.constructWcs_ = function(token, onReady) {
  remoting.wcs = new remoting.Wcs(
      remoting.wcsLoader.wcsIqClient,
      token,
      function() { onReady(token); });
};
