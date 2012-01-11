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
 * @param {Object.<string>} paramHash The parameter key/value pairs.
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
 * Execute an XHR GET asynchronously.
 *
 * @param {string} url The base URL to GET, excluding parameters.
 * @param {function(XMLHttpRequest):void} onDone The function to call on
 *     completion.
 * @param {(string|Object.<string>)=} opt_parameters The request parameters,
 *     either as an associative array, or a string.  If it is a string, do
 *     not include the ? and be sure it is correctly URLEncoded.
 * @param {Object.<string>=} opt_headers Additional headers to include on the
 *     request.
 * @param {boolean=} opt_withCredentials Set the withCredentials flags in the
 *     XHR.
 * @return {XMLHttpRequest} The request object.
 */
remoting.xhr.get = function(url, onDone, opt_parameters, opt_headers,
                            opt_withCredentials) {
  return remoting.xhr.doMethod('GET', url, onDone, opt_parameters,
                               opt_headers, opt_withCredentials);
};

/**
 * Execute an XHR POST asynchronously.
 *
 * @param {string} url The base URL to POST, excluding parameters.
 * @param {function(XMLHttpRequest):void} onDone The function to call on
 *     completion.
 * @param {(string|Object.<string>)=} opt_parameters The request parameters,
 *     either as an associative array, or a string.  If it is a string, be
 *     sure it is correctly URLEncoded.
 * @param {Object.<string>=} opt_headers Additional headers to include on the
 *     request.
 * @param {boolean=} opt_withCredentials Set the withCredentials flags in the
 *     XHR.
 * @return {XMLHttpRequest} The request object.
 */
remoting.xhr.post = function(url, onDone, opt_parameters, opt_headers,
                             opt_withCredentials) {
  return remoting.xhr.doMethod('POST', url, onDone, opt_parameters,
                               opt_headers, opt_withCredentials);
};

/**
 * Execute an XHR DELETE asynchronously.
 *
 * @param {string} url The base URL to DELETE, excluding parameters.
 * @param {function(XMLHttpRequest):void} onDone The function to call on
 *     completion.
 * @param {(string|Object.<string>)=} opt_parameters The request parameters,
 *     either as an associative array, or a string.  If it is a string, be
 *     sure it is correctly URLEncoded.
 * @param {Object.<string>=} opt_headers Additional headers to include on the
 *     request.
 * @param {boolean=} opt_withCredentials Set the withCredentials flags in the
 *     XHR.
 * @return {XMLHttpRequest} The request object.
 */
remoting.xhr.remove = function(url, onDone, opt_parameters, opt_headers,
                             opt_withCredentials) {
  return remoting.xhr.doMethod('DELETE', url, onDone, opt_parameters,
                               opt_headers, opt_withCredentials);
};

/**
 * Execute an XHR PUT asynchronously.
 *
 * @param {string} url The base URL to PUT, excluding parameters.
 * @param {function(XMLHttpRequest):void} onDone The function to call on
 *     completion.
 * @param {(string|Object.<string>)=} opt_parameters The request parameters,
 *     either as an associative array, or a string.  If it is a string, be
 *     sure it is correctly URLEncoded.
 * @param {Object.<string>=} opt_headers Additional headers to include on the
 *     request.
 * @param {boolean=} opt_withCredentials Set the withCredentials flags in the
 *     XHR.
 * @return {XMLHttpRequest} The request object.
 */
remoting.xhr.put = function(url, onDone, opt_parameters, opt_headers,
                             opt_withCredentials) {
  return remoting.xhr.doMethod('PUT', url, onDone, opt_parameters,
                               opt_headers, opt_withCredentials);
};

/**
 * Execute an arbitrary HTTP method asynchronously.
 *
 * @param {string} methodName The HTTP method name, e.g. "GET", "POST" etc.
 * @param {string} url The base URL, excluding parameters.
 * @param {function(XMLHttpRequest):void} onDone The function to call on
 *     completion.
 * @param {(string|Object.<string>)=} opt_parameters The request parameters,
 *     either as an associative array, or a string.  If it is a string, be
 *     sure it is correctly URLEncoded.
 * @param {Object.<string>=} opt_headers Additional headers to include on the
 *     request.
 * @param {boolean=} opt_withCredentials Set the withCredentials flags in the
 *     XHR.
 * @return {XMLHttpRequest} The XMLHttpRequest object.
 */
remoting.xhr.doMethod = function(methodName, url, onDone,
                                 opt_parameters, opt_headers,
                                 opt_withCredentials) {
  /** @type {XMLHttpRequest} */
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    onDone(xhr);
  };

  var parameterString = '';
  if (typeof(opt_parameters) === 'string') {
    parameterString = opt_parameters;
  } else if (typeof(opt_parameters) === 'object') {
    parameterString = remoting.xhr.urlencodeParamHash(opt_parameters);
  } else if (opt_parameters === undefined) {
    // No problem here. Do nothing.
  } else {
    throw 'opt_parameters must be string or associated array.';
  }

  var useBody = (methodName == 'POST') || (methodName == 'PUT');

  if (!useBody && parameterString != '') {
    url = url + '?' + parameterString;
  }

  xhr.open(methodName, url, true);
  if (methodName == 'POST') {
    xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
  }
  // Add in request headers.
  if (typeof(opt_headers) === 'object') {
    for (var key in opt_headers) {
      xhr.setRequestHeader(key, opt_headers[key]);
    }
  } else if (opt_headers === undefined) {
    // No problem here. Do nothing.
  } else {
    throw 'opt_headers must be associative array.';
  }

  if (opt_withCredentials) {
    xhr.withCredentials = true;
  }

  xhr.send(useBody ? parameterString : null);
  return xhr;
};
