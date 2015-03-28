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
  remoting.Xhr.checkParams_(params);

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
    url += '?' + parameterString;
  }

  // Prepare the build modified headers.
  /** @const */
  this.headers_ = remoting.Xhr.removeNullFields_(params.headers);

  // Convert the content fields to a single text content variable.
  /** @private {?string} */
  this.content_ = null;
  if (params.textContent !== undefined) {
    this.maybeSetContentType_('text/plain');
    this.content_ = params.textContent;
  } else if (params.formContent !== undefined) {
    this.maybeSetContentType_('application/x-www-form-urlencoded');
    this.content_ = remoting.Xhr.urlencodeParamHash(params.formContent);
  } else if (params.jsonContent !== undefined) {
    this.maybeSetContentType_('application/json');
    this.content_ = JSON.stringify(params.jsonContent);
  }

  // Apply the oauthToken field.
  if (params.oauthToken !== undefined) {
    this.setAuthToken_(params.oauthToken);
  }

  /** @private @const {boolean} */
  this.acceptJson_ = params.acceptJson || false;
  if (this.acceptJson_) {
    this.maybeSetHeader_('Accept', 'application/json');
  }

  // Apply useIdentity field.
  /** @const {boolean} */
  this.useIdentity_ = params.useIdentity || false;

  /** @private @const {!XMLHttpRequest} */
  this.nativeXhr_ = new XMLHttpRequest();
  this.nativeXhr_.onreadystatechange = this.onReadyStateChange_.bind(this);
  this.nativeXhr_.withCredentials = params.withCredentials || false;
  this.nativeXhr_.open(params.method, url, true);

  /** @private {base.Deferred<!remoting.Xhr.Response>} */
  this.deferred_ = null;
};

/**
 * Parameters for the 'start' function.  Unless otherwise noted, all
 * parameters are optional.
 *
 * method: (required) The HTTP method to use.
 *
 * url: (required) The URL to request.
 *
 * urlParams: Parameters to be appended to the URL.  Null-valued
 *     parameters are omitted.
 *
 * textContent: Text to be sent as the request body.
 *
 * formContent: Data to be URL-encoded and sent as the request body.
 *     Causes Content-type header to be set appropriately.
 *
 * jsonContent: Data to be JSON-encoded and sent as the request body.
 *     Causes Content-type header to be set appropriately.
 *
 * headers: Additional request headers to be sent.  Null-valued
 *     headers are omitted.
 *
 * withCredentials: Value of the XHR's withCredentials field.
 *
 * oauthToken: An OAuth2 token used to construct an Authentication
 *     header.
 *
 * useIdentity: Use identity API to get an OAuth2 token.
 *
 * acceptJson: If true, send an Accept header indicating that a JSON
 *     response is expected.
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
 *   useIdentity:(boolean|undefined),
 *   acceptJson:(boolean|undefined)
 * }}
 */
remoting.Xhr.Params;

/**
 * Starts and HTTP request and gets a promise that is resolved when
 * the request completes.
 *
 * Any error that prevents sending the request causes the promise to
 * be rejected.
 *
 * NOTE: Calling this method more than once will return the same
 * promise and not start a new request, despite what the name
 * suggests.
 *
 * @return {!Promise<!remoting.Xhr.Response>}
 */
remoting.Xhr.prototype.start = function() {
  if (this.deferred_ == null) {
    this.deferred_ = new base.Deferred();

    // Send the XHR, possibly after getting an OAuth token.
    var that = this;
    if (this.useIdentity_) {
      remoting.identity.getToken().then(function(token) {
        base.debug.assert(that.nativeXhr_.readyState == 1);
        that.setAuthToken_(token);
        that.sendXhr_();
      }).catch(function(error) {
        that.deferred_.reject(error);
      });
    } else {
      this.sendXhr_();
    }
  }
  return this.deferred_.promise();
};

/**
 * @param {remoting.Xhr.Params} params
 * @throws {Error} if params are invalid
 */
remoting.Xhr.checkParams_ = function(params) {
  if (params.urlParams) {
    if (params.url.indexOf('?') != -1) {
      throw new Error('URL may not contain "?" when urlParams is set');
    }
    if (params.url.indexOf('#') != -1) {
      throw new Error('URL may not contain "#" when urlParams is set');
    }
  }

  if ((Number(params.textContent !== undefined) +
       Number(params.formContent !== undefined) +
       Number(params.jsonContent !== undefined)) > 1) {
    throw new Error(
        'may only specify one of textContent, formContent, and jsonContent');
  }

  if (params.useIdentity && params.oauthToken !== undefined) {
    throw new Error('may not specify both useIdentity and oauthToken');
  }

  if ((params.useIdentity || params.oauthToken !== undefined) &&
      params.headers &&
      params.headers['Authorization'] != null) {
    throw new Error(
        'may not specify useIdentity or oauthToken ' +
        'with an Authorization header');
  }
};

/**
 * @param {string} token
 * @private
 */
remoting.Xhr.prototype.setAuthToken_ = function(token) {
  this.setHeader_('Authorization', 'Bearer ' + token);
};

/**
 * @param {string} type
 * @private
 */
remoting.Xhr.prototype.maybeSetContentType_ = function(type) {
  this.maybeSetHeader_('Content-type', type + '; charset=UTF-8');
};

/**
 * @param {string} key
 * @param {string} value
 * @private
 */
remoting.Xhr.prototype.setHeader_ = function(key, value) {
  var wasSet = this.maybeSetHeader_(key, value);
  base.debug.assert(wasSet);
};

/**
 * @param {string} key
 * @param {string} value
 * @return {boolean}
 * @private
 */
remoting.Xhr.prototype.maybeSetHeader_ = function(key, value) {
  if (!(key in this.headers_)) {
    this.headers_[key] = value;
    return true;
  }
  return false;
};

/** @private */
remoting.Xhr.prototype.sendXhr_ = function() {
  for (var key in this.headers_) {
    this.nativeXhr_.setRequestHeader(key, this.headers_[key]);
  }
  this.nativeXhr_.send(this.content_);
  this.content_ = null;  // for gc
};

/**
 * @private
 */
remoting.Xhr.prototype.onReadyStateChange_ = function() {
  var xhr = this.nativeXhr_;
  if (xhr.readyState == 4) {
    // See comments at remoting.Xhr.Response.
    this.deferred_.resolve(new remoting.Xhr.Response(
        xhr, this.acceptJson_));
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
 * @param {boolean} allowJson
 */
remoting.Xhr.Response = function(xhr, allowJson) {
  /** @private @const */
  this.allowJson_ = allowJson;

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

  /** @private {*|undefined}  */
  this.json_ = undefined;
};

/**
 * @return {boolean} True if the response code is outside the 200-299
 *     range (i.e. success as defined by the HTTP protocol).
 */
remoting.Xhr.Response.prototype.isError = function() {
  return this.status < 200 || this.status >= 300;
};

/**
 * @return {string} The text content of the response.
 */
remoting.Xhr.Response.prototype.getText = function() {
  return this.text_;
};

/**
 * Get the JSON content of the response.  Requires acceptJson to have
 * been true in the request.
 * @return {*} The parsed JSON content of the response.
 */
remoting.Xhr.Response.prototype.getJson = function() {
  base.debug.assert(this.allowJson_);
  if (this.json_ === undefined) {
    this.json_ = JSON.parse(this.text_);
  }
  return this.json_;
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
