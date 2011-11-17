// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class representing the host-list portion of the home screen UI.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 */
remoting.Host = function() {
  /** @type {string} */
  this.hostName = '';
  /** @type {string} */
  this.hostId = '';
  /** @type {string} */
  this.status = '';
  /** @type {string} */
  this.jabberId = '';
  /** @type {string} */
  this.publicKey = '';
}


/**
 * @constructor
 * @param {Element} table The HTML <table> to contain host-list.
 * @param {Element} errorDiv The HTML <div> to display error messages.
 */
remoting.HostList = function(table, errorDiv) {
  this.table = table;
  this.errorDiv = errorDiv;
  /** @type {Array.<remoting.Host>} */
  this.hosts = null;
}

/**
 * Refresh the host list with up-to-date details.
 * @param {Array.<remoting.Host>} hosts The new host list.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.update = function(hosts) {
  this.table.innerHTML = '';
  this.showError(null);
  this.hosts = hosts;

  for (var i = 0; i < hosts.length; ++i) {
    var host = hosts[i];
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

    this.table.appendChild(hostEntry);
  }

  this.showOrHide_(this.hosts.length != 0);
}

/**
 * Display a localized error message.
 * @param {remoting.Error?} errorTag The error to display, or NULL to clear any
 *     previous error.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.showError = function(errorTag) {
  this.table.innerHTML = '';
  if (errorTag) {
    l10n.localizeElementFromTag(this.errorDiv,
                                /** @type {string} */ (errorTag));
    this.showOrHide_(true);
  } else {
    this.errorDiv.innerText = '';
  }
}

/**
 * Show or hide the host-list UI.
 * @param {boolean} show True to show the UI, or false to hide it.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.showOrHide_ = function(show) {
  var parent = /** @type {Element} */ (this.table.parentNode);
  parent.hidden = !show;
  if (show) {
    parent.style.height = parent.scrollHeight + 'px';
    removeClass(parent, remoting.HostList.COLLAPSED_);
  } else {
    addClass(parent, remoting.HostList.COLLAPSED_);
  }
}

/**
 * Class name for the host list when it is collapsed.
 * @private
 */
remoting.HostList.COLLAPSED_ = 'collapsed';

/** @type {remoting.HostList} */
remoting.hostList = null;