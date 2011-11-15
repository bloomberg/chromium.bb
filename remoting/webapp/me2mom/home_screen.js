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

/**
 * Cache of the latest host list and status information.
 *
 * @type {Array.<{hostName: string, hostId: string, status: string,
 *                jabberId: string, publicKey: string}>}
 *
 */
remoting.hostList = new Array();

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
          showHostListError_(remoting.Error.AUTHENTICATION_FAILED);
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
        replaceHostList_(parsed_response.data.items);
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
        showHostListError_(remoting.Error.GENERIC);
      }
    }
  } catch (er) {
    console.error('Error processing response: ', xhr);
  }
}

/**
 * Refresh the host list display with up to date host details.
 *
 * @param {Array.<{hostName: string, hostId: string, status: string,
 *                jabberId: string, publicKey: string}>} hostList
 *     The new list of registered hosts.
 * @return {void} Nothing.
 */
function replaceHostList_(hostList) {
  var hostListDiv = document.getElementById('host-list-div');
  var hostListTable = document.getElementById('host-list');

  remoting.hostList = hostList;

  // Clear the table before adding the host info.
  hostListTable.innerHTML = '';
  showHostListError_(null);

  for (var i = 0; i < hostList.length; ++i) {
    var host = hostList[i];
    if (!host.hostName || !host.hostId || !host.status || !host.jabberId ||
        !host.publicKey)
      continue;
    var hostEntry = document.createElement('tr');
    addClass(hostEntry, 'host-list-row');

    var hostIcon = document.createElement('td');
    var hostIconImage = document.createElement('img');
    hostIconImage.src = 'icon_host.png';
    hostIcon.className = 'host-list-row-start';
    hostIcon.appendChild(hostIconImage);
    hostEntry.appendChild(hostIcon);

    var hostName = document.createElement('td');
    hostName.setAttribute('class', 'mode-select-label');
    hostName.appendChild(document.createTextNode(host.hostName));
    hostEntry.appendChild(hostName);

    var hostStatus = document.createElement('td');
    if (host.status == 'ONLINE') {
      var connectButton = document.createElement('button');
      connectButton.setAttribute('class', 'mode-select-button');
      connectButton.setAttribute('type', 'button');
      connectButton.setAttribute('onclick',
                                 'remoting.connectHost("'+host.hostId+'")');
      connectButton.innerHTML =
          chrome.i18n.getMessage(/*i18n-content*/'CONNECT_BUTTON');
      hostStatus.appendChild(connectButton);
    } else {
      addClass(hostEntry, 'host-offline');
      hostStatus.innerHTML = chrome.i18n.getMessage(/*i18n-content*/'OFFLINE');
    }
    hostStatus.className = 'host-list-row-end';
    hostEntry.appendChild(hostStatus);

    hostListTable.appendChild(hostEntry);
  }

  showHostList_(hostList.length != 0);
}

/**
 * Show or hide the host list.
 * @param {boolean} show True to show the list or false to hide it.
 * @return {void}
 */
function showHostList_(show) {
  var hostListDiv = document.getElementById('host-list-div');
  hostListDiv.hidden = (!show);
  if (show) {
    hostListDiv.style.height = hostListDiv.scrollHeight + 'px';
    removeClass(hostListDiv, 'collapsed');
  } else {
    addClass(hostListDiv, 'collapsed');
  }
}

/**
 * Show an error message in the host list portion of the UI.
 *
 * @param {remoting.Error?} errorTag The error to display, or NULL to clear any
 *     previous error.
 * @return {void} Nothing.
 */
function showHostListError_(errorTag) {
  var hostListTable = document.getElementById('host-list');
  hostListTable.innerHTML = '';
  var errorDiv = document.getElementById('host-list-error');
  if (errorTag) {
    l10n.localizeElementFromTag(errorDiv, /** @type {string} */ (errorTag));
    showHostList_(true);
  } else {
    errorDiv.innerText = '';
  }
}

}());