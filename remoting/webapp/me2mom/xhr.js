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
remoting.xhr = remoting.xhr || {};

(function() {

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
}

/**
 * Execute an XHR GET asynchronously.
 *
 * @param {string} url The base URL to GET, excluding parameters.
 * @param {function(XMLHttpRequest):void} onDone The function to call on
 *     completion.
 * @param {(string|Object.<string>)} opt_parameters The request parameters,
 *     either as an associative array, or a string.  If it is a string, do
 *     not include the ? and be sure it is correctly URLEncoded.
 * @param {Object.<string>} opt_headers Additional headers to include on the
 *     request.
 * @param {boolean} opt_withCredentials Set the withCredentials flags in the
 *     XHR.
 * @return {void} Nothing.
 */
remoting.xhr.get = function(url, onDone, opt_parameters, opt_headers,
                            opt_withCredentials) {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    onDone(xhr);
  };

  // Add parameters into URL.
  if (typeof(opt_parameters) === 'string') {
    if (opt_parameters.length > 0) {
      url = url + '?' + opt_parameters;
    }
  } else if (typeof(opt_parameters) === 'object') {
    var paramString = remoting.xhr.urlencodeParamHash(opt_parameters);
    if (paramString.length > 0) {
      url = url + '?' + paramString;
    }
  } else if (opt_parameters === undefined) {
    // No problem here. Do nothing.
  } else {
    throw 'opt_parameters must be string or associated array.';
  }

  xhr.open('GET', url, true);

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

  xhr.send(null);
}

/**
 * Execute an XHR POST asynchronously.
 *
 * @param {string} url The base URL to POST, excluding parameters.
 * @param {function(XMLHttpRequest):void} onDone The function to call on
 *     completion.
 * @param {(string|Object.<string>)} opt_parameters The request parameters,
 *     either as an associative array, or a string.  If it is a string, be
 *     sure it is correctly URLEncoded.
 * @param {Object.<string>} opt_headers Additional headers to include on the
 *     request.
 * @param {boolean} opt_withCredentials Set the withCredentials flags in the
 *     XHR.
 * @return {void} Nothing.
 */
remoting.xhr.post = function(url, onDone, opt_parameters, opt_headers,
                             opt_withCredentials) {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    onDone(xhr);
  };

  // Add parameters into URL.
  var postData = '';
  if (typeof(opt_parameters) === 'string') {
    postData = opt_parameters;
  } else if (typeof(opt_parameters) === 'object') {
    postData = remoting.xhr.urlencodeParamHash(opt_parameters);
  } else if (opt_parameters === undefined) {
    // No problem here. Do nothing.
  } else {
    throw 'opt_parameters must be string or associated array.';
  }

  xhr.open('POST', url, true);
  xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');

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

  xhr.send(postData);
}

}());
