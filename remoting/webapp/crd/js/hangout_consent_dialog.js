// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

var instance_ = null;

/**
 * Shows a dialog to ask for the user's permission to accept remote assistance
 * from a Hangout participant.
 *
 * @constructor
 * @private
 */
remoting.HangoutConsentDialog = function() {
  /**
   * @type {base.Deferred}
   * @private
   */
  this.onConsentResponseDeferred_ = null;

  /** @private */
  this.requestToken_ = base.generateXsrfToken();

  base.Ipc.getInstance().register('remoting.HangoutConsentDialog.confirm',
                                  this.onConfirmResponse_.bind(this));
};

/**
 * @param {boolean} confirm Whether the user authorized the it2me connection
 * @param {string} responseToken
 * @private
 */
remoting.HangoutConsentDialog.prototype.onConfirmResponse_ = function(
    confirm, responseToken) {
  if (responseToken !== this.requestToken_) {
    return;
  }

  if (confirm) {
    this.onConsentResponseDeferred_.resolve();
  } else {
    this.onConsentResponseDeferred_.reject(
        new Error(remoting.Error.CANCELLED));
  }
  this.onConsentResponseDeferred_ = null;
};

/**
 * @param {boolean} authorized If true, the consent dialog will hide the
 *  authorization section.
 * @param {Bounds=} opt_parentBounds If present, the consent dialog will be
 *  centered within |opt_parentBounds|.
 * @return {Promise} A Promise that will resolve when permission is granted or
 *   reject if the user cancels.
 */
remoting.HangoutConsentDialog.prototype.show = function(authorized,
                                                        opt_parentBounds) {
  if (!this.onConsentResponseDeferred_) {
    var DIALOG_WIDTH = 750;
    var DIALOG_HEIGHT = 480;

    var createOptions = {
      frame: 'none',
      resizable: false,
      outerBounds: { width: DIALOG_WIDTH, height: DIALOG_HEIGHT }
    };

    var params = {
      token: this.requestToken_,
      authorized: Boolean(authorized)
    };

    var url = base.urlJoin('dialog_hangout_consent.html', params);

    if (opt_parentBounds) {
      // Center the dialog on the parent bounds.
      var rect = opt_parentBounds;
      createOptions.outerBounds.top =
          Math.round(rect.top + rect.height / 2 - DIALOG_HEIGHT / 2);
      createOptions.outerBounds.left =
          Math.round(rect.left + rect.width / 2 - DIALOG_WIDTH / 2);
    }

    this.onConsentResponseDeferred_ = new base.Deferred();
    chrome.app.window.create(url, createOptions);
  }
  return this.onConsentResponseDeferred_.promise();
};

/** @return {remoting.HangoutConsentDialog} */
remoting.HangoutConsentDialog.getInstance = function() {
  if (!instance_) {
    instance_ = new remoting.HangoutConsentDialog();
  }
  return instance_;
};

}());

