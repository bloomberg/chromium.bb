// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
* @fileoverview
* The entry point for dialog_hangout_consent.html.
*/


/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * @constructor
 * @param {HTMLElement} rootElement
 * @param {string} requestToken
 * @param {boolean} authorized Whether the user is authorized or not.
 */
var ConsentDialog = function(rootElement, requestToken, authorized) {
  /** @private */
  this.okButton_ = rootElement.querySelector('.ok-button');
  /** @private */
  this.cancelButton_ = rootElement.querySelector('.cancel-button');
  /** @private */
  this.authSection_ = rootElement.querySelector('.auth-message');
  /** @private */
  this.requestToken_ = requestToken;

  if (authorized) {
    this.authSection_.hidden = true;
  }

  this.okButton_.addEventListener('click', this.onButton_.bind(this, true));
  this.cancelButton_.addEventListener('click',this.onButton_.bind(this, false));
  base.resizeWindowToContent();
};

/**
 * @param {boolean} confirm
 * @private
 */
ConsentDialog.prototype.onButton_ = function(confirm) {
  base.Ipc.invoke('remoting.HangoutConsentDialog.confirm', confirm,
                  this.requestToken_);
  chrome.app.window.current().close();
};

function onDomContentLoaded() {
  var params = base.getUrlParameters();
  var authorized = getBooleanAttr(params, 'authorized', false);
  var token = getStringAttr(params, 'token');
  l10n.localize();
  var confirmDialog = new ConsentDialog(document.body, token, authorized);
}

window.addEventListener('load', onDomContentLoaded);

}());
