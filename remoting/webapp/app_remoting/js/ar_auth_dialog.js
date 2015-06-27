// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

var instance_ = null;

/**
 * @constructor
 * @implements {remoting.Identity.ConsentDialog}
 * @private
 */
remoting.AuthDialog = function() {
  /** @private {base.Deferred} */
  this.deferred_ = null;
};

/**
 * @return {Promise} A Promise object that resolves when the user clicks on the
 *   auth button.
 */
remoting.AuthDialog.prototype.show = function() {
  if (!this.deferred_) {
    this.deferred_ = new base.Deferred();
    remoting.MessageWindow.showMessageWindow(
        l10n.getTranslationOrError(/*i18n-content*/'MODE_AUTHORIZE'),
        l10n.getTranslationOrError(/*i18n-content*/'DESCRIPTION_AUTHORIZE'),
        l10n.getTranslationOrError(/*i18n-content*/'CONTINUE_BUTTON'),
        this.onOk_.bind(this));
  }
  return this.deferred_.promise();
};

/**
 * @return {remoting.AuthDialog}
 */
remoting.AuthDialog.getInstance = function() {
  if (!instance_) {
    instance_ = new remoting.AuthDialog();
  }
  return instance_;
};

remoting.AuthDialog.prototype.onOk_ = function() {
  console.assert(this.deferred_ !== null, 'No deferred Promise found.');
  this.deferred_.resolve();
  this.deferred_ = null;
};

})();
