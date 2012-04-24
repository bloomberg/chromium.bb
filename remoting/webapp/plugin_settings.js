// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains all the settings that may need massaging by the build script.
// Keeping all that centralized here allows us to use symlinks for the other
// files making for a faster compile/run cycle when only modifying HTML/JS.

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @type {string} The MIME type of the Host plugin.
 */
remoting.PLUGIN_MIMETYPE = 'HOST_PLUGIN_MIMETYPE';

/**
 * @type {boolean} Flag that indicates whether official client ID should
 *                 be used.
 */
remoting.OAuth2.prototype.USE_OFFICIAL_CLIENT_ID =
    OAUTH2_USE_OFFICIAL_CLIENT_ID;

/**
 * @type {string} The OAuth2 redirect URL.
 * @private
 */
remoting.OAuth2.prototype.REDIRECT_URI_ = 'OAUTH2_REDIRECT_URL';

// Constants for parameters used in retrieving the OAuth2 credentials.
/** @private */
remoting.OAuth2.prototype.CLIENT_ID_ = 'API_CLIENT_ID';
/** @private */
remoting.OAuth2.prototype.CLIENT_SECRET_ = 'API_CLIENT_SECRET';
