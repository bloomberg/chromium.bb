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
 * @type {string} The OAuth2 redirect URL.
 * @private
 */
remoting.OAuth2.prototype.REDIRECT_URI_ = 'OAUTH2_REDIRECT_URL';
