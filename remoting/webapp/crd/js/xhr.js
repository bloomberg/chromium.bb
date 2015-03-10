// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Simple utilities for making XHRs more pleasant.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** Namespace for XHR functions */
/** @type {Object} */
remoting.xhr = remoting.xhr || {};

/**
 * Takes an associative array of parameters and urlencodes it.
 *
 * @param {Object<string,string>} paramHash The parameter key/value pairs.
 * @return {string} URLEncoded version of paramHash.
 */
remoting.xhr.urlencodeParamHash = function(paramHash) {
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
 * Parameters for the 'start' function.
 *
 * method: The HTTP method to use.
 *
 * url: The URL to request.
 *
 * onDone: Function to call when the XHR finishes.

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
 * @typedef {{
 *   method: string,
 *   url:string,
 *   onDone:(function(XMLHttpRequest):void),
 *   urlParams:(string|Object<string,?string>|undefined),
 *   textContent:(string|undefined),
 *   formContent:(Object|undefined),
 *   jsonContent:(*|undefined),
 *   headers:(Object<string,?string>|undefined),
 *   withCredentials:(boolean|undefined),
 *   oauthToken:(string|undefined)
 * }}
 */
remoting.XhrParams;

/**
 * Returns a copy of the input object with all null or undefined
 * fields removed.
 *
 * @param {Object<string,?string>|undefined} input
 * @return {!Object<string,string>}
 * @private
 */
remoting.xhr.removeNullFields_ = function(input) {
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
 * Executes an arbitrary HTTP method asynchronously.
 *
 * @param {remoting.XhrParams} params
 * @return {XMLHttpRequest} The XMLHttpRequest object.
 */
remoting.xhr.start = function(params) {
  // Extract fields that can be used more or less as-is.
  var method = params.method;
  var url = params.url;
  var onDone = params.onDone;
  var headers = remoting.xhr.removeNullFields_(params.headers);
  var withCredentials = params.withCredentials || false;

  // Apply URL parameters.
  var parameterString = '';
  if (typeof(params.urlParams) === 'string') {
    parameterString = params.urlParams;
  } else if (typeof(params.urlParams) === 'object') {
    parameterString = remoting.xhr.urlencodeParamHash(
        remoting.xhr.removeNullFields_(params.urlParams));
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

  // Convert the content fields to a single text content variable.
  /** @type {?string} */
  var content = null;
  if (params.textContent !== undefined) {
    content = params.textContent;
  } else if (params.formContent !== undefined) {
    if (!('Content-type' in headers)) {
      headers['Content-type'] = 'application/x-www-form-urlencoded';
    }
    content = remoting.xhr.urlencodeParamHash(params.formContent);
  } else if (params.jsonContent !== undefined) {
    if (!('Content-type' in headers)) {
      headers['Content-type'] = 'application/json; charset=UTF-8';
    }
    content = JSON.stringify(params.jsonContent);
  }

  // Apply the oauthToken field.
  if (params.oauthToken !== undefined) {
    base.debug.assert(!('Authorization' in headers));
    headers['Authorization'] = 'Bearer ' + params.oauthToken;
  }

  return remoting.xhr.startInternal_(
      method, url, onDone, content, headers, withCredentials);
};

/**
 * Executes an arbitrary HTTP method asynchronously.
 *
 * @param {string} method
 * @param {string} url
 * @param {function(XMLHttpRequest):void} onDone
 * @param {?string} content
 * @param {!Object<string,string>} headers
 * @param {boolean} withCredentials
 * @return {XMLHttpRequest} The XMLHttpRequest object.
 * @private
 */
remoting.xhr.startInternal_ = function(
    method, url, onDone, content, headers, withCredentials) {
  /** @type {XMLHttpRequest} */
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    onDone(xhr);
  };

  xhr.open(method, url, true);
  for (var key in headers) {
    xhr.setRequestHeader(key, headers[key]);
  }
  xhr.withCredentials = withCredentials;
  xhr.send(content);
  return xhr;
};

/**
 * Generic success/failure response proxy.
 *
 * @param {function():void} onDone
 * @param {function(!remoting.Error):void} onError
 * @return {function(XMLHttpRequest):void}
 */
remoting.xhr.defaultResponse = function(onDone, onError) {
  /** @param {XMLHttpRequest} xhr */
  var result = function(xhr) {
    var error =
        remoting.Error.fromHttpStatus(/** @type {number} */ (xhr.status));
    if (!error.isError()) {
      onDone();
    } else {
      onError(error);
    }
  };
  return result;
};
