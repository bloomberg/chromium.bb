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
 * Create a host list consisting of the specified HTML elements, which should
 * have a common parent that contains only host-list UI as it will be hidden
 * if the host-list is empty.
 * @constructor
 * @param {Element} table The HTML <table> to contain host-list.
 * @param {Element} errorDiv The HTML <div> to display error messages.
 */
remoting.HostList = function(table, errorDiv) {
  /**
   * @type {Element}
   * @private
   */
  this.table_ = table;
  /**
   * @type {Element}
   * @private
   */
  this.errorDiv_ = errorDiv;
  /**
   * @type {Array.<remoting.HostTableEntry>}
   * @private
   */
  this.hostTableEntries_ = null;
};

/**
 * Search the host list for a host with the specified id.
 * @param {string} hostId The unique id of the host.
 * @return {remoting.HostTableEntry?} The host table entry, if any.
 */
remoting.HostList.prototype.getHostForId = function(hostId) {
  for (var i = 0; i < this.hostTableEntries_.length; ++i) {
    if (this.hostTableEntries_[i].host.hostId == hostId) {
      return this.hostTableEntries_[i];
    }
  }
  return null;
};

/**
 * Query the Remoting Directory for the user's list of hosts.
 *
 * @return {void} Nothing.
 */
remoting.HostList.prototype.refresh = function() {
  /** @type {remoting.HostList} */
  var that = this;
  /** @param {XMLHttpRequest} xhr */
  var parseHostListResponse = function(xhr) {
    that.parseHostListResponse_(xhr);
  }
  /** @param {string} token */
  var getHosts = function(token) {
    var headers = { 'Authorization': 'OAuth ' + token };
    remoting.xhr.get(
        'https://www.googleapis.com/chromoting/v1/@me/hosts',
        parseHostListResponse, '', headers);
  };
  remoting.oauth2.callWithToken(getHosts);
}

/**
 * Handle the results of the host list request.  A success response will
 * include a JSON-encoded list of host descriptions, which we display if we're
 * able to successfully parse it.
 *
 * @param {XMLHttpRequest} xhr The XHR object for the host list request.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.parseHostListResponse_ = function(xhr) {
  try {
    if (xhr.status == 200) {
      var parsed_response =
          /** @type {{data: {items: Array}}} */ JSON.parse(xhr.responseText);
      if (parsed_response.data && parsed_response.data.items) {
        this.setHosts_(parsed_response.data.items);
      }
    } else {
      // Some other error.
      console.error('Bad status on host list query: ', xhr);
      // For most errors in the 4xx range, tell the user to re-authorize us.
      if (xhr.status == 403) {
        // The user's account is not enabled for Me2Me, so fail silently.
      } else if (xhr.status >= 400 && xhr.status <= 499) {
        this.showError_(remoting.Error.GENERIC);
      }
    }
  } catch (er) {
    console.error('Error processing response: ', xhr, er);
  }
}

/**
 * Refresh the host list with up-to-date details.
 * @param {Array.<remoting.Host>} hosts The new host list.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.setHosts_ = function(hosts) {
  this.table_.innerHTML = '';
  this.showError_(null);
  this.hostTableEntries_ = [];

  /** @type {remoting.HostList} */
  var that = this;
  for (var i = 0; i < hosts.length; ++i) {
    /** @type {remoting.Host} */
    var host = hosts[i];
    // Validate the entry to make sure it has all the fields we expect.
    if (host.hostName && host.hostId && host.status && host.jabberId &&
        host.publicKey) {
      var onRename = function() { that.renameHost_(host.hostId); }
      var onDelete = function() { that.deleteHost_(host.hostId); }
      var hostTableEntry = new remoting.HostTableEntry();
      hostTableEntry.init(host, onRename, onDelete);
      this.hostTableEntries_[i] = hostTableEntry;
      this.table_.appendChild(hostTableEntry.tableRow);
    }
  }

  this.showOrHide_(this.hostTableEntries_.length != 0);
};

/**
 * Display a localized error message.
 * @param {remoting.Error?} errorTag The error to display, or NULL to clear any
 *     previous error.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.showError_ = function(errorTag) {
  this.table_.innerHTML = '';
  if (errorTag) {
    l10n.localizeElementFromTag(this.errorDiv_,
                                /** @type {string} */ (errorTag));
    this.showOrHide_(true);
  } else {
    this.errorDiv_.innerText = '';
  }
};

/**
 * Show or hide the host-list UI.
 * @param {boolean} show True to show the UI, or false to hide it.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.showOrHide_ = function(show) {
  var parent = /** @type {Element} */ (this.table_.parentNode);
  parent.hidden = !show;
  if (show) {
    parent.style.height = parent.scrollHeight + 'px';
    removeClass(parent, remoting.HostList.COLLAPSED_);
  } else {
    addClass(parent, remoting.HostList.COLLAPSED_);
  }
};

/**
 * Remove a host from the list, and deregister it.
 * @param {string} hostId The id of the host to be removed.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.deleteHost_ = function(hostId) {
  /** @type {remoting.HostTableEntry} */
  var hostTableEntry = this.getHostForId(hostId);
  if (!hostTableEntry) {
    console.error('No host registered for id ' + hostId);
    return;
  }

  this.table_.removeChild(hostTableEntry.tableRow);
  var index = this.hostTableEntries_.indexOf(hostTableEntry);
  if (index != -1) {  // Since we've just found it, index must be >= 0
    this.hostTableEntries_.splice(index, 1);
  }

  /** @param {string} token */
  var deleteHost = function(token) {
    var headers = { 'Authorization': 'OAuth ' + token };
    remoting.xhr.remove(
        'https://www.googleapis.com/chromoting/v1/@me/hosts/' + hostId,
        function() {}, '', headers);
  }
  remoting.oauth2.callWithToken(deleteHost);

  this.showOrHide_(this.hostTableEntries_.length != 0);
};

/**
 * Prepare a host for renaming by replacing its name with an edit box.
 * @param {string} hostId The id of the host to be renamed.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.renameHost_ = function(hostId) {
  /** @type {remoting.HostTableEntry} */
  var hostTableEntry = this.getHostForId(hostId);
  if (!hostTableEntry) {
    console.error('No host registered for id ' + hostId);
    return;
  }
  /** @param {string} token */
  var renameHost = function(token) {
    var headers = {
      'Authorization': 'OAuth ' + token,
      'Content-type' : 'application/json; charset=UTF-8'
    };
    var newHostDetails = { data: hostTableEntry.host };
    remoting.xhr.put(
        'https://www.googleapis.com/chromoting/v1/@me/hosts/' + hostId,
        function(xhr) {},
        JSON.stringify(newHostDetails),
        headers);
  }
  remoting.oauth2.callWithToken(renameHost);
};

/**
 * Class name for the host list when it is collapsed.
 * @private
 */
remoting.HostList.COLLAPSED_ = 'collapsed';

/** @type {remoting.HostList} */
remoting.hostList = null;
