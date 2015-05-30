// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

var COUNTDOWN_IN_SECONDS = 10;

/**
 * A Dialog that is shown to the user when the connection is dropped.
 *
 * @constructor
 * @param {HTMLElement} rootElement
 * @param {remoting.WindowShape=} opt_windowShape
 * @implements {base.Disposable}
 */
remoting.ConnectionDroppedDialog = function(rootElement, opt_windowShape) {
  /** @private */
  this.dialog_ = new remoting.Html5ModalDialog({
    dialog: /** @type {HTMLDialogElement} */ (rootElement),
    primaryButton: rootElement.querySelector('.restart-button'),
    secondaryButton: rootElement.querySelector('.close-button'),
    closeOnEscape: false,
    windowShape: opt_windowShape
  });

  /** @private {HTMLElement} */
  this.countdownElement_ = rootElement.querySelector('.restart-timer');
  this.countdownElement_.hidden = true;

  /** @private {base.Disposable} */
  this.reconnectTimer_ = null;

  /** @private {number} */
  this.countdown_;

  /** @private {base.Disposable} */
  this.onlineEventHook_;
};

remoting.ConnectionDroppedDialog.prototype.dispose = function() {
  base.dispose(this.reconnectTimer_);
  this.onlineEventHook_ = null;
  base.dispose(this.reconnectTimer_);
  this.reconnectTimer_ = null;
  base.dispose(this.dialog_);
  this.dialog_ = null;
};

/**
 * @return {Promise}  Promise that resolves if
 *     the user chooses to reconnect, or rejects if the user chooses to cancel.
 *     This class doesn't re-establish the connection.  The caller must
 *     implement the reconnect logic.
 */
remoting.ConnectionDroppedDialog.prototype.show = function() {
  var promise = this.dialog_.show();
  this.waitForOnline_().then(
    this.startReconnectTimer_.bind(this)
  );
  return promise.then(function(/** remoting.MessageDialog.Result */ result) {
    if (result === remoting.MessageDialog.Result.PRIMARY) {
      return Promise.resolve();
    } else {
      return Promise.reject(new remoting.Error(remoting.Error.Tag.CANCELLED));
    }
  });
};

/**
 * @private
 * @return {Promise}
 */
remoting.ConnectionDroppedDialog.prototype.waitForOnline_ = function() {
  if (navigator.onLine) {
    return Promise.resolve();
  }

  var deferred = new base.Deferred();
  this.onlineEventHook_ = new base.DomEventHook(
      window, 'online', deferred.resolve.bind(deferred), false);
  return deferred.promise();
};

/** @private */
remoting.ConnectionDroppedDialog.prototype.onCountDown_ = function() {
  if (this.countdown_ === 0) {
    base.dispose(this.reconnectTimer_);
    this.dialog_.close(remoting.MessageDialog.Result.PRIMARY);
  }

  this.countdown_--;
  var pad = (this.countdown_ < 10) ? '0:0' : '0:';
  l10n.localizeElement(this.countdownElement_, pad + this.countdown_);
};

/** @private */
remoting.ConnectionDroppedDialog.prototype.startReconnectTimer_ = function() {
  this.countdown_ = COUNTDOWN_IN_SECONDS;
  this.countdownElement_.hidden = false;
  this.reconnectTimer_ =
      new base.RepeatingTimer(this.onCountDown_.bind(this), 1000, true);
};

})();
