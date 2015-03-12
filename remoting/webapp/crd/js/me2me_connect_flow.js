// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * @param {HTMLElement} rootElement
 * @param {remoting.Host} host
 * @constructor
 */
remoting.HostNeedsUpdateDialog = function(rootElement, host) {
  /** @private */
  this.host_ = host;
  /** @private */
  this.rootElement_ = rootElement;
  /** @private {base.Deferred} */
  this.deferred_ = null;
  /** @private {base.Disposables} */
  this.eventHooks_ = null;

  l10n.localizeElementFromTag(
      rootElement.querySelector('.host-needs-update-message'),
      /*i18n-content*/'HOST_NEEDS_UPDATE_TITLE', host.hostName);
};

/**
 * Shows the HostNeedsUpdateDialog if the user is trying to connect to a legacy
 * host.
 *
 * @param {string} webappVersion
 * @return {Promise}  Promise that resolves when no update is required or
 *    when the user ignores the update warning.  Rejects with
 *    |remoting.Error.CANCELLED| if the user cancels the connection.
 */
remoting.HostNeedsUpdateDialog.prototype.showIfNecessary =
    function(webappVersion) {
  if (!remoting.Host.needsUpdate(this.host_, webappVersion)) {
    return Promise.resolve();
  }

  this.eventHooks_ = new base.Disposables(
    new base.DomEventHook(this.rootElement_.querySelector('.connect-button'),
                         'click', this.onOK_.bind(this), false),
    new base.DomEventHook(this.rootElement_.querySelector('.cancel-button'),
                          'click', this.onCancel_.bind(this), false));

  base.debug.assert(this.deferred_ === null);
  this.deferred_ = new base.Deferred();

  remoting.setMode(remoting.AppMode.CLIENT_HOST_NEEDS_UPGRADE);

  return this.deferred_.promise();
};

/** @private */
remoting.HostNeedsUpdateDialog.prototype.cleanup_ = function() {
  base.dispose(this.eventHooks_);
  this.eventHooks_ = null;
  this.deferred_ = null;
};


/** @private */
remoting.HostNeedsUpdateDialog.prototype.onOK_ = function() {
  this.deferred_.resolve();
  this.cleanup_();
};

/** @private */
remoting.HostNeedsUpdateDialog.prototype.onCancel_ = function() {
  this.deferred_.reject(remoting.Error.CANCELLED);
  this.cleanup_();
};

})();
