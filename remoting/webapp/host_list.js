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
 *
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
  this.hostTableEntries_ = [];
  /**
   * @type {Array.<remoting.Host>}
   * @private
   */
  this.hosts_ = [];
  /**
   * @type {string}
   * @private
   */
  this.lastError_ = '';

  // Load the cache of the last host-list, if present.
  var cached = /** @type {string} */
      (window.localStorage.getItem(remoting.HostList.HOSTS_KEY));
  if (cached) {
    try {
      this.hosts_ = /** @type {Array} */ JSON.parse(cached);
    } catch (err) {
      console.error('Invalid host list cache:', /** @type {*} */(err));
    }
  }
};

/**
 * Search the host list for a host with the specified id.
 *
 * @param {string} hostId The unique id of the host.
 * @return {remoting.Host?} The host, if any.
 */
remoting.HostList.prototype.getHostForId = function(hostId) {
  for (var i = 0; i < this.hosts_.length; ++i) {
    if (this.hosts_[i].hostId == hostId) {
      return this.hosts_[i];
    }
  }
  return null;
};

/**
 * Query the Remoting Directory for the user's list of hosts.
 *
 * @param {function(boolean):void} onDone Callback invoked with true on success
 *     or false on failure.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.refresh = function(onDone) {
  /** @type {remoting.HostList} */
  var that = this;
  /** @param {XMLHttpRequest} xhr The response from the server. */
  var parseHostListResponse = function(xhr) {
    that.parseHostListResponse_(xhr, onDone);
  }
  /** @param {string} token The OAuth2 token. */
  var getHosts = function(token) {
    var headers = { 'Authorization': 'OAuth ' + token };
    remoting.xhr.get(
        'https://www.googleapis.com/chromoting/v1/@me/hosts',
        parseHostListResponse, '', headers);
  };
  remoting.oauth2.callWithToken(getHosts);
};

/**
 * Handle the results of the host list request.  A success response will
 * include a JSON-encoded list of host descriptions, which we display if we're
 * able to successfully parse it.
 *
 * @param {XMLHttpRequest} xhr The XHR object for the host list request.
 * @param {function(boolean):void} onDone The callback passed to |refresh|.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.parseHostListResponse_ = function(xhr, onDone) {
  this.hosts_ = [];
  this.lastError_ = '';
  try {
    if (xhr.status == 200) {
      var parsed_response =
          /** @type {{data: {items: Array}}} */ JSON.parse(xhr.responseText);
      if (parsed_response.data && parsed_response.data.items) {
        this.hosts_ = parsed_response.data.items;
      }
    } else {
      // Some other error.
      console.error('Bad status on host list query: ', xhr);
      if (xhr.status == 403) {
        // The user's account is not enabled for Me2Me, so fail silently.
      } else if (xhr.status >= 400 && xhr.status < 500) {
        // For other errors, tell the user to re-authorize us.
        this.lastError_ = remoting.Error.GENERIC;
      } else if (xhr.status == 503) {
        this.lastError_ = remoting.Error.SERVICE_UNAVAILABLE;
      } else {
        this.lastError_ = remoting.Error.UNEXPECTED;
      }
    }
  } catch (er) {
    var typed_er = /** @type {Object} */ (er);
    console.error('Error processing response: ', xhr, typed_er);
    this.lastError_ = remoting.Error.UNEXPECTED;
  }
  window.localStorage.setItem(remoting.HostList.HOSTS_KEY,
                              JSON.stringify(this.hosts_));
  onDone(this.lastError_ == '');
};

/**
 * Display the list of hosts or error condition.
 *
 * @return {void} Nothing.
 */
remoting.HostList.prototype.display = function() {
  this.table_.innerHTML = '';
  this.errorDiv_.innerText = '';
  this.hostTableEntries_ = [];

  /**
   * @type {remoting.HostList}
   */
  var that = this;
  /**
   * @param {remoting.HostTableEntry} hostTableEntry The entry being renamed.
   */
  var onRename = function(hostTableEntry) { that.renameHost_(hostTableEntry); }
  /**
   * @param {remoting.HostTableEntry} hostTableEntry The entry beign deleted.
   */
  var onDelete = function(hostTableEntry) { that.deleteHost_(hostTableEntry); }

  for (var i = 0; i < this.hosts_.length; ++i) {
    /** @type {remoting.Host} */
    var host = this.hosts_[i];
    // Validate the entry to make sure it has all the fields we expect.
    if (host.hostName && host.hostId && host.status && host.jabberId &&
        host.publicKey) {
      var hostTableEntry = new remoting.HostTableEntry();
      hostTableEntry.init(host, onRename, onDelete);
      this.hostTableEntries_[i] = hostTableEntry;
      this.table_.appendChild(hostTableEntry.tableRow);
    }
  }

  if (this.lastError_ != '') {
    l10n.localizeElementFromTag(this.errorDiv_, this.lastError_);
  }

  this.showOrHide_(this.hosts_.length != 0 || this.lastError_ != '');
};

/**
 * Show or hide the host-list UI.
 *
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
 * @param {remoting.HostTableEntry} hostTableEntry The host to be removed.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.deleteHost_ = function(hostTableEntry) {
  this.table_.removeChild(hostTableEntry.tableRow);
  var index = this.hostTableEntries_.indexOf(hostTableEntry);
  if (index != -1) {
    this.hostTableEntries_.splice(index, 1);
  }

  /** @param {string} token The OAuth2 token. */
  var deleteHost = function(token) {
    var headers = { 'Authorization': 'OAuth ' + token };
    remoting.xhr.remove(
        'https://www.googleapis.com/chromoting/v1/@me/hosts/' +
        hostTableEntry.host.hostId,
        function() {}, '', headers);
  }
  remoting.oauth2.callWithToken(deleteHost);

  this.showOrHide_(this.hostTableEntries_.length != 0);
};

/**
 * Prepare a host for renaming by replacing its name with an edit box.
 * @param {remoting.HostTableEntry} hostTableEntry The host to be renamed.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.renameHost_ = function(hostTableEntry) {
  /** @param {string} token */
  var renameHost = function(token) {
    var headers = {
      'Authorization': 'OAuth ' + token,
      'Content-type' : 'application/json; charset=UTF-8'
    };
    var newHostDetails = { data: {
      hostId: hostTableEntry.host.hostId,
      hostName: hostTableEntry.host.hostName,
      publicKey: hostTableEntry.host.publicKey
    } };
    remoting.xhr.put(
        'https://www.googleapis.com/chromoting/v1/@me/hosts/' +
        hostTableEntry.host.hostId,
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

/**
 * Key name under which Me2Me hosts are cached.
 */
remoting.HostList.HOSTS_KEY = 'me2me-cached-hosts';

/** @type {remoting.HostList} */
remoting.hostList = null;
