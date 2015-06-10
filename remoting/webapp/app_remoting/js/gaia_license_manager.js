// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

"use strict";

/**
 * A simple implementation of AppRemotingAuth using the user's GAIA token.
 *
 * @implements{remoting.LicenseManager}
 * @constructor
 */
remoting.GaiaLicenseManager = function() {};

/**
 * @param {string} oauthToken
 * @return {Promise<!string>}
 */
remoting.GaiaLicenseManager.prototype.getSubscriptionToken =
    function(oauthToken) {
  return Promise.resolve(oauthToken);
};

/**
 * @param {string} oauthToken
 * @return {Promise<!string>}
 */
remoting.GaiaLicenseManager.prototype.getAccessToken = function(oauthToken) {
  return Promise.resolve("");
};

})();

