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
  this.deferred_.reject(new remoting.Error(remoting.Error.Tag.CANCELLED));
  this.cleanup_();
};

/**
 * @param {HTMLElement} rootElement
 * @param {remoting.Host} host
 * @constructor
 */
remoting.PinDialog = function(rootElement, host) {
  /** @private */
  this.rootElement_ = rootElement;
  /** @private {base.Deferred} */
  this.deferred_ = null;
  /** @private {base.Disposables} */
  this.eventHooks_ = null;
  /** @private */
  this.pairingCheckbox_ = rootElement.querySelector('.pairing-checkbox');
  /** @private */
  this.pinInput_ = rootElement.querySelector('.pin-inputField');
  /** @private */
  this.host_ = host;
};

/**
 * @param {boolean} supportsPairing
 * @return {Promise<string>}  Promise that resolves with the PIN or rejects with
 *    |remoting.Error.CANCELLED| if the user cancels the connection.
 */
remoting.PinDialog.prototype.show = function(supportsPairing) {
  var cancel = this.rootElement_.querySelector('.cancel-button');
  var form = this.rootElement_.querySelector('form');
  var onCancel = this.createClickHandler_(this.onCancel_.bind(this));
  var onConnect = this.createClickHandler_(this.onConnect_.bind(this));

  this.eventHooks_ = new base.Disposables(
    new base.DomEventHook(form, 'submit', onConnect, false),
    new base.DomEventHook(cancel, 'click', onCancel, false));

  // Reset the UI.
  this.pairingCheckbox_.checked = false;
  this.rootElement_.querySelector('.pairing-section').hidden = !supportsPairing;
  var message = this.rootElement_.querySelector('.pin-message');
  l10n.localizeElement(message, this.host_.hostName);
  this.pinInput_.value = '';

  base.debug.assert(this.deferred_ === null);
  this.deferred_ = new base.Deferred();

  remoting.setMode(remoting.AppMode.CLIENT_PIN_PROMPT);

  return this.deferred_.promise();
};

/** @return {boolean} */
remoting.PinDialog.prototype.pairingRequested = function() {
  return this.pairingCheckbox_.checked;
};

/**
 * @param {function():void} handler
 * @return {Function}
 * @private
 */
remoting.PinDialog.prototype.createClickHandler_ = function(handler) {
  var that = this;
  return function (/** Event */ e) {
    // Prevents form submission from reloading the v1 app.
    e.preventDefault();

    // Set the focus away from the password field. This has to be done
    // before the password field gets hidden, to work around a Blink
    // clipboard-handling bug - http://crbug.com/281523.
    that.rootElement_.querySelector('.connect-button').focus();
    handler();
    base.dispose(that.eventHooks_);
    that.eventHooks_ = null;
    that.deferred_ = null;
  };
};

/** @private */
remoting.PinDialog.prototype.onConnect_ = function() {
  this.deferred_.resolve(this.pinInput_.value);
  remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
};

/** @private */
remoting.PinDialog.prototype.onCancel_ = function() {
  this.deferred_.reject(new remoting.Error(remoting.Error.Tag.CANCELLED));
  remoting.setMode(remoting.AppMode.HOME);
};

})();
