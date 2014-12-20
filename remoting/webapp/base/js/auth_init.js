// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Get the user's email address and full name.
 *
 * @param {function(string,string):void} onUserInfoAvailable Callback invoked
 *     when the user's email address and full name are available.
 * @return {void} Nothing.
 */
remoting.initIdentity = function(onUserInfoAvailable) {

  /**
   * Show the authorization consent UI and register a one-shot event handler to
   * continue the authorization process.
   *
   * @param {function():void} authContinue Callback to invoke when the user
   *     clicks "Continue".
   */
  function promptForConsent(authContinue) {
    /** @type {HTMLElement} */
    var dialog = document.getElementById('auth-dialog');
    /** @type {HTMLElement} */
    var button = document.getElementById('auth-button');
    var consentGranted = function(event) {
      dialog.hidden = true;
      button.removeEventListener('click', consentGranted, false);
      authContinue();
      remoting.windowShape.updateClientWindowShape();
    };
    dialog.hidden = false;

    /** @type {HTMLElement} */
    var dialog_border = document.getElementById('auth-dialog-border');
    remoting.authDialog = new remoting.AuthDialog(dialog_border);
    remoting.windowShape.addCallback(remoting.authDialog);

    button.addEventListener('click', consentGranted, false);
  }

  /** @param {remoting.Error} error */
  function onGetIdentityInfoError(error) {
    // No need to show the error message for NOT_AUTHENTICATED
    // because we will show "auth-dialog".
    if (error != remoting.Error.NOT_AUTHENTICATED) {
      remoting.showErrorMessage(error);
    }
  }

  if (base.isAppsV2()) {
    remoting.identity = new remoting.Identity(promptForConsent);
  } else {
    // TODO(garykac) Remove this and replace with identity.
    remoting.oauth2 = new remoting.OAuth2();
    if (!remoting.oauth2.isAuthenticated()) {
      document.getElementById('auth-dialog').hidden = false;
    }
    remoting.identity = remoting.oauth2;
  }

  remoting.identity.getUserInfo(onUserInfoAvailable,
                                onGetIdentityInfoError);
}
