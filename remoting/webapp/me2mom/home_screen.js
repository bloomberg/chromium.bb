// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Functions related to the 'home screen' for Chromoting.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

/**
 * Query the Remoting Directory for the user's list of hosts.
 *
 * @return {void} Nothing.
 */
remoting.refreshHostList = function() {
  // Fetch a new Access Token for the user, if necessary.
  if (remoting.oauth2.needsNewAccessToken()) {
    remoting.oauth2.refreshAccessToken(function(xhr) {
        if (remoting.oauth2.needsNewAccessToken()) {
          // Failed to get access token
          console.error('refreshHostList: OAuth2 token fetch failed');
          remoting.hostList.showError(remoting.Error.AUTHENTICATION_FAILED);
          return;
        }
        remoting.refreshHostList();
      });
    return;
  }

  var headers = {
    'Authorization': 'OAuth ' + remoting.oauth2.getAccessToken()
  };

  var xhr = remoting.xhr.get(
      'https://www.googleapis.com/chromoting/v1/@me/hosts',
      parseHostListResponse_,
      '',
      headers);
}

/**
 * Handle the results of the host list request.  A success response will
 * include a JSON-encoded list of host descriptions, which we display if we're
 * able to successfully parse it.
 *
 * @param {XMLHttpRequest} xhr The XHR object for the host list request.
 * @return {void} Nothing.
 */
function parseHostListResponse_(xhr) {
  // Ignore host list responses if we're not on the Home screen. This mainly
  // ensures that errors don't cause an unexpected mode switch.
  if (remoting.currentMode != remoting.AppMode.HOME) {
    return;
  }

  if (xhr.readyState != 4) {
    return;
  }

  try {
    if (xhr.status == 200) {
      var parsed_response =
          /** @type {{data: {items: Array}}} */ JSON.parse(xhr.responseText);
      if (parsed_response.data && parsed_response.data.items) {
        remoting.hostList.update(parsed_response.data.items);
      }
    } else {
      // Some other error.  Log for now, pretty-print in future.
      console.error('Bad status on host list query: ', xhr);
      var errorResponse =
          /** @type {{error: {code: *, message: *}}} */
          JSON.parse(xhr.responseText);
      if (errorResponse.error &&
          errorResponse.error.code &&
          errorResponse.error.message) {
        remoting.debug.log('Error code ' + errorResponse.error.code);
        remoting.debug.log('Error message ' + errorResponse.error.message);
      } else {
        remoting.debug.log('Error response: ' + xhr.responseText);
      }

      // For most errors in the 4xx range, tell the user to re-authorize us.
      if (xhr.status == 403) {
        // The user's account is not enabled for Me2Me, so fail silently.
      } else if (xhr.status >= 400 && xhr.status <= 499) {
        remoting.hostList.showError(remoting.Error.GENERIC);
      }
    }
  } catch (er) {
    console.error('Error processing response: ', xhr);
  }
}

}());