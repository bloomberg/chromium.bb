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
  REGISTRATION_FAILED: /*i18n-content*/'ERROR_HOST_REGISTRATION_FAILED'
};
