// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @enum {string} All error messages from messages.json
 */
remoting.Error = {
  NONE: '',

  // Used to signify that an operation was cancelled by the user. This should
  // not normally cause the error text to be shown to the user, so the
  // i18n-content prefix is not needed in this case.
  CANCELLED: '__CANCELLED__',

  INVALID_ACCESS_CODE: /*i18n-content*/'ERROR_INVALID_ACCESS_CODE',
  MISSING_PLUGIN: /*i18n-content*/'ERROR_MISSING_PLUGIN',
  AUTHENTICATION_FAILED: /*i18n-content*/'ERROR_AUTHENTICATION_FAILED',
  HOST_IS_OFFLINE: /*i18n-content*/'ERROR_HOST_IS_OFFLINE',
  INCOMPATIBLE_PROTOCOL: /*i18n-content*/'ERROR_INCOMPATIBLE_PROTOCOL',
  BAD_PLUGIN_VERSION: /*i18n-content*/'ERROR_BAD_PLUGIN_VERSION',
  NETWORK_FAILURE: /*i18n-content*/'ERROR_NETWORK_FAILURE',
  HOST_OVERLOAD: /*i18n-content*/'ERROR_HOST_OVERLOAD',
  UNEXPECTED: /*i18n-content*/'ERROR_UNEXPECTED',
  SERVICE_UNAVAILABLE: /*i18n-content*/'ERROR_SERVICE_UNAVAILABLE',
  NOT_AUTHENTICATED: /*i18n-content*/'ERROR_NOT_AUTHENTICATED',
  INVALID_HOST_DOMAIN: /*i18n-content*/'ERROR_INVALID_HOST_DOMAIN',
  P2P_FAILURE: /*i18n-content*/'ERROR_P2P_FAILURE',
  REGISTRATION_FAILED: /*i18n-content*/'ERROR_HOST_REGISTRATION_FAILED',
  NOT_AUTHORIZED: /*i18n-content*/'ERROR_NOT_AUTHORIZED',

  // TODO(garykac): Move app-specific errors into separate location.
  APP_NOT_AUTHORIZED: /*i18n-content*/'ERROR_APP_NOT_AUTHORIZED'
};

/**
 * @param {number} httpStatus An HTTP status code.
 * @return {remoting.Error} The remoting.Error enum corresponding to the
 *     specified HTTP status code.
 */
remoting.Error.fromHttpStatus = function(httpStatus) {
  if (httpStatus == 0) {
    return remoting.Error.NETWORK_FAILURE;
  } else if (httpStatus >= 200 && httpStatus < 300) {
    return remoting.Error.NONE;
  } else if (httpStatus == 400 || httpStatus == 401) {
    return remoting.Error.AUTHENTICATION_FAILED;
  } else if (httpStatus >= 500 && httpStatus < 600) {
    return remoting.Error.SERVICE_UNAVAILABLE;
  } else {
    console.warn('Unexpected HTTP error code: ' + httpStatus);
    // Return AUTHENTICATION_FAILED by default, so that the user can try to
    // recover from an unexpected failure by signing in again.
    // TODO(jamiewalch): Return UNEXPECTED here and let calling code treat that
    // as "sign-in required" if necessary.
    return remoting.Error.AUTHENTICATION_FAILED;
  }
};
