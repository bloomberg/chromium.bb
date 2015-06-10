// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * @interface
 */
remoting.LicenseManager = function() {};

/**
 * Called by App Streaming to obtain a fresh Subscription Token to pass to the
 * Orchestrator to authorize access to the Vendor’s application.
 * The returned Promise should emit the token serialized into a string, suitable
 * for the App Streaming client to deliver to the VM.
 *
 * @param {string} oauthToken Identity Token identifying the user for which a
 *     Subscription token is being requested.
 * @return {Promise<!string>}
 */
remoting.LicenseManager.prototype.getSubscriptionToken = function(oauthToken){};

/**
 * Called by App Streaming to obtain a fresh Access Token to pass to the
 * application VM for use by the application to access services provided by the
 * Vendor.
 * The returned Promise should emit the token serialized into a string, suitable
 * for the App Streaming client to deliver to the VM.
 * NOTE: This interface may be revised to allow for supporting e.g. client-bound
 * Access Tokens in future.
 *
 * @param {string} oauthToken Identity Token identifying the user for which an
 *     Access Token is being requested.
 * @return {Promise<!string>}
 */
remoting.LicenseManager.prototype.getAccessToken = function(oauthToken) {};

})();



