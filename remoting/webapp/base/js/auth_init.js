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

  /** @param {remoting.Error} error */
  function onGetIdentityInfoError(error) {
    // No need to show the error message for NOT_AUTHENTICATED
    // because we will show "auth-dialog".
    if (error != remoting.Error.NOT_AUTHENTICATED) {
      remoting.showErrorMessage(error);
    }
  }

  if (base.isAppsV2()) {
    remoting.identity =
        new remoting.Identity(remoting.AuthDialog.getInstance());
  } else {
    // TODO(garykac) Remove this and replace with identity.
    remoting.oauth2 = new remoting.OAuth2();
    var oauth2 = /** @type {*} */ (remoting.oauth2);
    remoting.identity = /** @type {remoting.Identity} */ (oauth2);
    if (!remoting.identity.isAuthenticated()) {
      remoting.AuthDialog.getInstance().show().then(function() {
        remoting.oauth2.doAuthRedirect(function(){
          window.location.reload();
        });
      });
    }
  }

  remoting.identity.getUserInfo().then(
      /** @param {{email:string, name:string}} userInfo */
      function(userInfo) {
        onUserInfoAvailable(userInfo.email, userInfo.name);
      }).catch(function(error) {
        onGetIdentityInfoError(
            /** @type {remoting.Error} */ (error));
      });
};

/**
 * Removes the cached token and restarts the app.
 *
 * @return {void}  Nothing.
 */
remoting.handleAuthFailureAndRelaunch = function() {
  remoting.identity.removeCachedAuthToken().then(function(){
    if (base.isAppsV2()) {
      base.Ipc.invoke('remoting.ActivationHandler.restart',
                      chrome.app.window.current().id);
    } else {
      window.location.reload();
    }
  });
};
