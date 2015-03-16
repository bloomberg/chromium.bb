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
 * @param {string=} opt_detail
 */
remoting.Error = function(tag, opt_detail) {
  /** @private @const {remoting.Error.Tag} */
  this.tag_ = tag;

  /** @const {?string} */
  this.detail_ = opt_detail || null;
};

/**
 * @override
 */
remoting.Error.prototype.toString = function() {
  var result = this.tag_;
  if (this.detail_ != null) {
    result += ' (' + this.detail_ + ')';
  }
  return result;
};

/**
 * @return {remoting.Error.Tag} The tag used to create this Error.
 */
remoting.Error.prototype.getTag = function() {
  return this.tag_;
};

/**
 * Checks the type of an error.
 * @param {remoting.Error.Tag} tag
 * @param {...remoting.Error.Tag} var_args
 * @return {boolean} True if this object has one of the specified tags.
 */
remoting.Error.prototype.hasTag = function(tag, var_args) {
  var thisTag = this.tag_;
  return Array.prototype.some.call(
      arguments,
      function(/** remoting.Error.Tag */ tag) {
        return thisTag == tag;
      });
};

/**
 * @return {boolean} True if this object's tag is NONE, meaning this
 *     object represents the lack of an error.
 */
remoting.Error.prototype.isNone = function() {
  return this.hasTag(remoting.Error.Tag.NONE);
};

/**
 * Convenience method for creating the second most common error type.
 * @return {!remoting.Error}
 */
remoting.Error.none = function() {
  return new remoting.Error(remoting.Error.Tag.NONE);
};

/**
 * Convenience method for creating the most common error type.
 * @return {!remoting.Error}
 */
remoting.Error.unexpected = function() {
  return new remoting.Error(remoting.Error.Tag.UNEXPECTED);
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

/**
 * @param {number} httpStatus An HTTP status code.
 * @return {!remoting.Error} The remoting.Error enum corresponding to the
 *     specified HTTP status code.
 */
remoting.Error.fromHttpStatus = function(httpStatus) {
  if (httpStatus == 0) {
    return new remoting.Error(remoting.Error.Tag.NETWORK_FAILURE);
  } else if (httpStatus >= 200 && httpStatus < 300) {
    return remoting.Error.none();
  } else if (httpStatus == 400 || httpStatus == 401) {
    return new remoting.Error(remoting.Error.Tag.AUTHENTICATION_FAILED);
  } else if (httpStatus == 403) {
    return new remoting.Error(remoting.Error.Tag.NOT_AUTHORIZED);
  } else if (httpStatus == 404) {
    return new remoting.Error(remoting.Error.Tag.NOT_FOUND);
  } else if (httpStatus >= 500 && httpStatus < 600) {
    return new remoting.Error(remoting.Error.Tag.SERVICE_UNAVAILABLE);
  } else {
    console.warn('Unexpected HTTP error code: ' + httpStatus);
    return remoting.Error.unexpected();
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
      console.error('Unexpected error:', error);
      onError(remoting.Error.unexpected());
    }
  };
};
