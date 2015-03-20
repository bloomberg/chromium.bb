// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Utility class for making XHRs more pleasant.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @param {remoting.Xhr.Params} params
 */
remoting.Xhr = function(params) {
  /** @private @const {!XMLHttpRequest} */
  this.nativeXhr_ = new XMLHttpRequest();
  this.nativeXhr_.onreadystatechange = this.onReadyStateChange_.bind(this);
  this.nativeXhr_.withCredentials = params.withCredentials || false;

  /** @private @const */
  this.responseType_ = params.responseType || remoting.Xhr.ResponseType.TEXT;

  // Apply URL parameters.
  var url = params.url;
  var parameterString = '';
  if (typeof(params.urlParams) === 'string') {
    parameterString = params.urlParams;
  } else if (typeof(params.urlParams) === 'object') {
    parameterString = remoting.Xhr.urlencodeParamHash(
        remoting.Xhr.removeNullFields_(params.urlParams));
  }
  if (parameterString) {
    base.debug.assert(url.indexOf('?') == -1);
    url += '?' + parameterString;
  }

  // Check that the content spec is consistent.
  if ((Number(params.textContent !== undefined) +
       Number(params.formContent !== undefined) +
       Number(params.jsonContent !== undefined)) > 1) {
    throw new Error(
        'may only specify one of textContent, formContent, and jsonContent');
  }

  // Prepare the build modified headers.
  var headers = remoting.Xhr.removeNullFields_(params.headers);

  // Convert the content fields to a single text content variable.
  /** @private {?string} */
  this.content_ = null;
  if (params.textContent !== undefined) {
    this.content_ = params.textContent;
  } else if (params.formContent !== undefined) {
    if (!('Content-type' in headers)) {
      headers['Content-type'] = 'application/x-www-form-urlencoded';
    }
    this.content_ = remoting.Xhr.urlencodeParamHash(params.formContent);
  } else if (params.jsonContent !== undefined) {
    if (!('Content-type' in headers)) {
      headers['Content-type'] = 'application/json; charset=UTF-8';
    }
    this.content_ = JSON.stringify(params.jsonContent);
  }

  // Apply the oauthToken field.
  if (params.oauthToken !== undefined) {
    base.debug.assert(!('Authorization' in headers));
    headers['Authorization'] = 'Bearer ' + params.oauthToken;
  }

  this.nativeXhr_.open(params.method, url, true);
  for (var key in headers) {
    this.nativeXhr_.setRequestHeader(key, headers[key]);
  }

  /** @private {base.Deferred<!remoting.Xhr.Response>} */
  this.deferred_ = null;
};

/**
 * @enum {string}
 */
remoting.Xhr.ResponseType = {
  TEXT: 'TEXT',  // Request a plain text response (default).
  JSON: 'JSON',  // Request a JSON response.
  NONE: 'NONE'   // Don't request any response.
};

/**
 * Parameters for the 'start' function.
 *
 * method: The HTTP method to use.
 *
 * url: The URL to request.
 *
 * urlParams: (optional) Parameters to be appended to the URL.
 *     Null-valued parameters are omitted.
 *
 * textContent: (optional) Text to be sent as the request body.
 *
 * formContent: (optional) Data to be URL-encoded and sent as the
 *     request body.  Causes Content-type header to be set
 *     appropriately.
 *
 * jsonContent: (optional) Data to be JSON-encoded and sent as the
 *     request body.  Causes Content-type header to be set
 *     appropriately.
 *
 * headers: (optional) Additional request headers to be sent.
 *     Null-valued headers are omitted.
 *
 * withCredentials: (optional) Value of the XHR's withCredentials field.
 *
 * oauthToken: (optional) An OAuth2 token used to construct an
 *     Authentication header.
 *
 * responseType: (optional) Request a response of a specific
 *    type. Default: TEXT.
 *
 * @typedef {{
 *   method: string,
 *   url:string,
 *   urlParams:(string|Object<string,?string>|undefined),
 *   textContent:(string|undefined),
 *   formContent:(Object|undefined),
 *   jsonContent:(*|undefined),
 *   headers:(Object<string,?string>|undefined),
 *   withCredentials:(boolean|undefined),
 *   oauthToken:(string|undefined),
 *   responseType:(remoting.Xhr.ResponseType|undefined)
 * }}
 */
remoting.Xhr.Params;

/**
 * Aborts the HTTP request.  Does nothing is the request has finished
 * already.
 */
remoting.Xhr.prototype.abort = function() {
  this.nativeXhr_.abort();
};

/**
 * Starts and HTTP request and gets a promise that is resolved when
 * the request completes.
 *
 * Any error that prevents receiving an HTTP status
 * code causes this promise to be rejected.
 *
 * NOTE: Calling this method more than once will return the same
 * promise and not start a new request, despite what the name
 * suggests.
 *
 * @return {!Promise<!remoting.Xhr.Response>}
 */
remoting.Xhr.prototype.start = function() {
  if (this.deferred_ == null) {
    var xhr = this.nativeXhr_;
    xhr.send(this.content_);
    this.content_ = null;  // for gc
    this.deferred_ = new base.Deferred();
  }
  return this.deferred_.promise();
};

/**
 * @private
 */
remoting.Xhr.prototype.onReadyStateChange_ = function() {
  var xhr = this.nativeXhr_;
  if (xhr.readyState == 4) {
    // See comments at remoting.Xhr.Response.
    this.deferred_.resolve(new remoting.Xhr.Response(xhr, this.responseType_));
  }
};

/**
 * The response-related parts of an XMLHttpRequest.  Note that this
 * class is not just a facade for XMLHttpRequest; it saves the value
 * of the |responseText| field becuase once onReadyStateChange_
 * (above) returns, the value of |responseText| is reset to the empty
 * string!  This is a documented anti-feature of the XMLHttpRequest
 * API.
 *
 * @constructor
 * @param {!XMLHttpRequest} xhr
 * @param {remoting.Xhr.ResponseType} type
 */
remoting.Xhr.Response = function(xhr, type) {
  /** @private @const */
  this.type_ = type;

  /**
   * The HTTP status code.
   * @const {number}
   */
  this.status = xhr.status;

  /**
   * The HTTP status description.
   * @const {string}
   */
  this.statusText = xhr.statusText;

  /**
   * The response URL, if any.
   * @const {?string}
   */
  this.url = xhr.responseURL;

  /** @private {string} */
  this.text_ = xhr.responseText || '';
};

/**
 * @return {string} The text content of the response.
 */
remoting.Xhr.Response.prototype.getText = function() {
  return this.text_;
};

/**
 * @return {*} The parsed JSON content of the response.
 */
remoting.Xhr.Response.prototype.getJson = function() {
  base.debug.assert(this.type_ == remoting.Xhr.ResponseType.JSON);
  return JSON.parse(this.text_);
};

/**
 * Returns a copy of the input object with all null or undefined
 * fields removed.
 *
 * @param {Object<string,?string>|undefined} input
 * @return {!Object<string,string>}
 * @private
 */
remoting.Xhr.removeNullFields_ = function(input) {
  /** @type {!Object<string,string>} */
  var result = {};
  if (input) {
    for (var field in input) {
      var value = input[field];
      if (value != null) {
        result[field] = value;
      }
    }
  }
  return result;
};

/**
 * Takes an associative array of parameters and urlencodes it.
 *
 * @param {Object<string,string>} paramHash The parameter key/value pairs.
 * @return {string} URLEncoded version of paramHash.
 */
remoting.Xhr.urlencodeParamHash = function(paramHash) {
  var paramArray = [];
  for (var key in paramHash) {
    paramArray.push(encodeURIComponent(key) +
                     '=' + encodeURIComponent(paramHash[key]));
  }
  if (paramArray.length > 0) {
    return paramArray.join('&');
  }
  return '';
};

/**
 * Generic success/failure response proxy.
 *
 * TODO(jrw): Stop using this and move default error handling directly
 * into Xhr class.
 *
 * @param {function():void} onDone
 * @param {function(!remoting.Error):void} onError
 * @param {Array<remoting.Error.Tag>=} opt_ignoreErrors
 * @return {function(!remoting.Xhr.Response):void}
 */
remoting.Xhr.defaultResponse = function(onDone, onError, opt_ignoreErrors) {
  /** @param {!remoting.Xhr.Response} response */
  var result = function(response) {
    var error = remoting.Error.fromHttpStatus(response.status);
    if (error.isNone()) {
      onDone();
      return;
    }

    if (opt_ignoreErrors && error.hasTag.apply(error, opt_ignoreErrors)) {
      onDone();
      return;
    }

    onError(error);
  };
  return result;
};
