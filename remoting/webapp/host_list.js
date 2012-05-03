// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  var cachedStr = /** @type {string} */
      (window.localStorage.getItem(remoting.HostList.HOSTS_KEY));
  if (cachedStr) {
    var cached = jsonParseSafe(cachedStr);
    if (cached) {
      this.hosts_ = /** @type {Array} */ cached;
    } else {
      console.error('Invalid value for ' + remoting.HostList.HOSTS_KEY);
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
  /** @param {string?} token The OAuth2 token. */
  var getHosts = function(token) {
    if (token) {
      // TODO(simonmorris): Pass the access token in a header, not a URL
      // parameter, when crbug.com/116574 has a better fix.
      var params = { 'access_token': token };
      remoting.xhr.get(
          'https://www.googleapis.com/chromoting/v1/@me/hosts',
          parseHostListResponse, params);
    } else {
      that.lastError_ = remoting.Error.AUTHENTICATION_FAILED;
      onDone(false);
    }
  };
  this.hosts_ = [];
  this.lastError_ = '';
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
  try {
    if (xhr.status == 200) {
      var response =
          /** @type {{data: {items: Array}}} */ jsonParseSafe(xhr.responseText);
      if (response && response.data && response.data.items) {
        this.hosts_ = response.data.items;
        /**
         * @param {remoting.Host} a
         * @param {remoting.Host} b
         */
        var cmp = function(a, b) {
          if (a.status < b.status) {
            return 1;
          } else if (b.status < a.status) {
            return -1;
          }
          return 0;
        };
        this.hosts_ = /** @type {Array} */ this.hosts_.sort(cmp);
      } else {
        console.error('Invalid "hosts" response from server.');
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
 * @param {string?} thisHostId The id of this host, or null if not registered.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.display = function(thisHostId) {
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
  var onRename = function(hostTableEntry) { that.renameHost(hostTableEntry); }
  /**
   * @param {remoting.HostTableEntry} hostTableEntry The entry beign deleted.
   */
  var onDelete = function(hostTableEntry) { that.deleteHost_(hostTableEntry); }

  this.table_.hidden = (this.hosts_.length == 0);

  for (var i = 0; i < this.hosts_.length; ++i) {
    /** @type {remoting.Host} */
    var host = this.hosts_[i];
    // Validate the entry to make sure it has all the fields we expect and is
    // not the local host (which is displayed separately). NB: if the host has
    // never sent a heartbeat, then there will be no jabberId.
    if (host.hostName && host.hostId && host.status && host.publicKey &&
        host.hostId != thisHostId) {
      var hostTableEntry = new remoting.HostTableEntry();
      hostTableEntry.create(host, onRename, onDelete);
      this.hostTableEntries_[i] = hostTableEntry;
      this.table_.appendChild(hostTableEntry.tableRow);
    }
  }

  if (this.lastError_ != '') {
    l10n.localizeElementFromTag(this.errorDiv_, this.lastError_);
  }
  this.errorDiv_.hidden = (this.lastError_ == '');
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
  remoting.HostList.unregisterHostById(hostTableEntry.host.hostId);
};

/**
 * Unregister a host.
 * @param {string} hostId The id of the host to be removed.
 * @return {void} Nothing.
 */
remoting.HostList.unregisterHostById = function(hostId) {
  /** @param {string?} token The OAuth2 token. */
  var deleteHost = function(token) {
    if (token) {
      var headers = { 'Authorization': 'OAuth ' + token };
      remoting.xhr.remove(
          'https://www.googleapis.com/chromoting/v1/@me/hosts/' + hostId,
          function() {}, '', headers);
    } else {
      console.error('Could not unregister host. Authentication failure.');
      // TODO(jamiewalch): Add a callback to signify success/failure?
    }
  }
  remoting.oauth2.callWithToken(deleteHost);
};

/**
 * Prepare a host for renaming by replacing its name with an edit box.
 * @param {remoting.HostTableEntry} hostTableEntry The host to be renamed.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.renameHost = function(hostTableEntry) {
  for (var i = 0; i < this.hosts_.length; ++i) {
    if (this.hosts_[i].hostId == hostTableEntry.host.hostId) {
      this.hosts_[i].hostName = hostTableEntry.host.hostName;
      break;
    }
  }
  window.localStorage.setItem(remoting.HostList.HOSTS_KEY,
                              JSON.stringify(this.hosts_));

  /** @param {string?} token */
  var renameHost = function(token) {
    if (token) {
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
    } else {
      console.error('Could not rename host. Authentication failure.');
    }
  }
  remoting.oauth2.callWithToken(renameHost);
};

/**
 * Add a host to the list. This is called when the local host is started to
 * avoid having to refresh the host list and deal with replication delays.
 *
 * @param {remoting.Host} localHost The local Me2Me host.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.addHost = function(localHost) {
  this.hosts_.push(localHost);
  window.localStorage.setItem(remoting.HostList.HOSTS_KEY,
                              JSON.stringify(this.hosts_));
}

/**
 * Key name under which Me2Me hosts are cached.
 */
remoting.HostList.HOSTS_KEY = 'me2me-cached-hosts';

/** @type {remoting.HostList} */
remoting.hostList = null;
