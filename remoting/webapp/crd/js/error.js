// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * A wrapper for remoting.Error.Tag.  Having a wrapper makes it
 * possible to use instanceof checks on caught exceptions.  It also
 * allows adding more detailed error information if desired.
 *
 * @constructor
 * @param {remoting.Error.Tag} tag
 * @param {string=} opt_message
 */
remoting.Error = function(tag, opt_message) {
  /** @const {remoting.Error.Tag} */
  this.tag = tag;

  /** @const {?string} */
  this.message = opt_message || null;
};

/**
 * @return {boolean} True if this object represents an error
 *     condition.
 */
remoting.Error.prototype.isError = function() {
  return this.tag != remoting.Error.Tag.NONE;
};

/**
 * @enum {string} All error messages from messages.json
 */
remoting.Error.Tag = {
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
  NOT_FOUND: /*i18n-content*/'ERROR_NOT_FOUND',
  INVALID_HOST_DOMAIN: /*i18n-content*/'ERROR_INVALID_HOST_DOMAIN',
  P2P_FAILURE: /*i18n-content*/'ERROR_P2P_FAILURE',
  REGISTRATION_FAILED: /*i18n-content*/'ERROR_HOST_REGISTRATION_FAILED',
  NOT_AUTHORIZED: /*i18n-content*/'ERROR_NOT_AUTHORIZED',

  // TODO(garykac): Move app-specific errors into separate location.
  APP_NOT_AUTHORIZED: /*i18n-content*/'ERROR_APP_NOT_AUTHORIZED'
};

// A whole bunch of semi-redundant constants, mostly to reduce to size
// of the diff that introduced the remoting.Error class.
//
// Please don't add any more constants here; just call the
// remoting.Error constructor directly

/** @const */
remoting.Error.NONE = new remoting.Error(remoting.Error.Tag.NONE);

/** @const */
remoting.Error.CANCELLED =
  new remoting.Error(remoting.Error.Tag.CANCELLED);

/** @const */
remoting.Error.INVALID_ACCESS_CODE =
  new remoting.Error(remoting.Error.Tag.INVALID_ACCESS_CODE);

/** @const */
remoting.Error.MISSING_PLUGIN =
  new remoting.Error(remoting.Error.Tag.MISSING_PLUGIN);

/** @const */
remoting.Error.AUTHENTICATION_FAILED =
  new remoting.Error(remoting.Error.Tag.AUTHENTICATION_FAILED);

/** @const */
remoting.Error.HOST_IS_OFFLINE =
  new remoting.Error(remoting.Error.Tag.HOST_IS_OFFLINE);

/** @const */
remoting.Error.INCOMPATIBLE_PROTOCOL =
  new remoting.Error(remoting.Error.Tag.INCOMPATIBLE_PROTOCOL);

/** @const */
remoting.Error.BAD_PLUGIN_VERSION =
  new remoting.Error(remoting.Error.Tag.BAD_PLUGIN_VERSION);

/** @const */
remoting.Error.NETWORK_FAILURE =
  new remoting.Error(remoting.Error.Tag.NETWORK_FAILURE);

/** @const */
remoting.Error.HOST_OVERLOAD =
  new remoting.Error(remoting.Error.Tag.HOST_OVERLOAD);

/** @const */
remoting.Error.UNEXPECTED =
  new remoting.Error(remoting.Error.Tag.UNEXPECTED);

/** @const */
remoting.Error.SERVICE_UNAVAILABLE =
  new remoting.Error(remoting.Error.Tag.SERVICE_UNAVAILABLE);

/** @const */
remoting.Error.NOT_AUTHENTICATED =
  new remoting.Error(remoting.Error.Tag.NOT_AUTHENTICATED);

/** @const */
remoting.Error.NOT_FOUND =
  new remoting.Error(remoting.Error.Tag.NOT_FOUND);

/** @const */
remoting.Error.INVALID_HOST_DOMAIN =
  new remoting.Error(remoting.Error.Tag.INVALID_HOST_DOMAIN);

/** @const */
remoting.Error.P2P_FAILURE =
  new remoting.Error(remoting.Error.Tag.P2P_FAILURE);

/** @const */
remoting.Error.REGISTRATION_FAILED =
  new remoting.Error(remoting.Error.Tag.REGISTRATION_FAILED);

/** @const */
remoting.Error.NOT_AUTHORIZED =
  new remoting.Error(remoting.Error.Tag.NOT_AUTHORIZED);

/** @const */
remoting.Error.APP_NOT_AUTHORIZED =
  new remoting.Error(remoting.Error.Tag.APP_NOT_AUTHORIZED);

/**
 * @param {number} httpStatus An HTTP status code.
 * @return {!remoting.Error} The remoting.Error enum corresponding to the
 *     specified HTTP status code.
 */
remoting.Error.fromHttpStatus = function(httpStatus) {
  if (httpStatus == 0) {
    return remoting.Error.NETWORK_FAILURE;
  } else if (httpStatus >= 200 && httpStatus < 300) {
    return remoting.Error.NONE;
  } else if (httpStatus == 400 || httpStatus == 401) {
    return remoting.Error.AUTHENTICATION_FAILED;
  } else if (httpStatus == 403) {
    return remoting.Error.NOT_AUTHORIZED;
  } else if (httpStatus == 404) {
    return remoting.Error.NOT_FOUND;
  } else if (httpStatus >= 500 && httpStatus < 600) {
    return remoting.Error.SERVICE_UNAVAILABLE;
  } else {
    console.warn('Unexpected HTTP error code: ' + httpStatus);
    return remoting.Error.UNEXPECTED;
  }
};

/**
 * Create an error-handling function suitable for passing to a
 * Promise's "catch" method.
 *
 * @param {function(!remoting.Error):void} onError
 * @return {function(*):void}
 */
remoting.Error.handler = function(onError) {
  return function(/** * */ error) {
    if (error instanceof remoting.Error) {
      onError(/** @type {!remoting.Error} */ (error));
    } else {
      console.error('Unexpected error: %o', error);
      onError(remoting.Error.UNEXPECTED);
    }
  };
};
