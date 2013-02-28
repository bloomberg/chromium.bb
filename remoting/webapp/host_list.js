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
 * @param {Element} table The HTML <div> to contain host-list.
 * @param {Element} noHosts The HTML <div> containing the "no hosts" message.
 * @param {Element} errorMsg The HTML <div> to display error messages.
 * @param {Element} errorButton The HTML <button> to display the error
 *     resolution action.
 */
remoting.HostList = function(table, noHosts, errorMsg, errorButton) {
  /**
   * @type {Element}
   * @private
   */
  this.table_ = table;
  /**
   * @type {Element}
   * @private
   * TODO(jamiewalch): This should be doable using CSS's sibling selector,
   * but it doesn't work right now (crbug.com/135050).
   */
  this.noHosts_ = noHosts;
  /**
   * @type {Element}
   * @private
   */
  this.errorMsg_ = errorMsg;
  /**
   * @type {Element}
   * @private
   */
  this.errorButton_ = errorButton;
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
  /**
   * @type {remoting.Host?}
   * @private
   */
  this.localHost_ = null;
  /**
   * @type {remoting.HostController.State}
   * @private
   */
  this.localHostState_ = remoting.HostController.State.NOT_IMPLEMENTED;
  /**
   * @type {number}
   * @private
   */
  this.webappMajorVersion_ = parseInt(chrome.runtime.getManifest().version, 10);

  this.errorButton_.addEventListener('click',
                                     this.onErrorClick_.bind(this),
                                     false);
};

/**
 * Load the host-list asynchronously from local storage.
 *
 * @param {function():void} onDone Completion callback.
 */
remoting.HostList.prototype.load = function(onDone) {
  // Load the cache of the last host-list, if present.
  /** @type {remoting.HostList} */
  var that = this;
  /** @param {Object.<string>} items */
  var storeHostList = function(items) {
    if (items[remoting.HostList.HOSTS_KEY]) {
      var cached = jsonParseSafe(items[remoting.HostList.HOSTS_KEY]);
      if (cached) {
        that.hosts_ = /** @type {Array} */ cached;
      } else {
        console.error('Invalid value for ' + remoting.HostList.HOSTS_KEY);
      }
    }
    onDone();
  };
  chrome.storage.local.get(remoting.HostList.HOSTS_KEY, storeHostList);
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
  /** @param {XMLHttpRequest} xhr The response from the server. */
  var parseHostListResponse = this.parseHostListResponse_.bind(this, onDone);
  /** @type {remoting.HostList} */
  var that = this;
  /** @param {string} token The OAuth2 token. */
  var getHosts = function(token) {
    var headers = { 'Authorization': 'OAuth ' + token };
    remoting.xhr.get(
        remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts',
        parseHostListResponse, '', headers);
  };
  /** @param {remoting.Error} error */
  var onError = function(error) {
    that.hosts_ = [];
    that.lastError_ = error;
    onDone(false);
  };
  remoting.identity.callWithToken(getHosts, onError);
};

/**
 * Handle the results of the host list request.  A success response will
 * include a JSON-encoded list of host descriptions, which we display if we're
 * able to successfully parse it.
 *
 * @param {function(boolean):void} onDone The callback passed to |refresh|.
 * @param {XMLHttpRequest} xhr The XHR object for the host list request.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.parseHostListResponse_ = function(onDone, xhr) {
  this.hosts_ = [];
  this.lastError_ = '';
  try {
    if (xhr.status == 200) {
      var response =
          /** @type {{data: {items: Array}}} */ jsonParseSafe(xhr.responseText);
      if (response && response.data) {
        if (response.data.items) {
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
        }
      } else {
        this.lastError_ = remoting.Error.UNEXPECTED;
        console.error('Invalid "hosts" response from server.');
      }
    } else {
      // Some other error.
      console.error('Bad status on host list query: ', xhr);
      if (xhr.status == 0) {
        this.lastError_ = remoting.Error.NO_RESPONSE;
      } else if (xhr.status == 401) {
        this.lastError_ = remoting.Error.AUTHENTICATION_FAILED;
      } else if (xhr.status == 502 || xhr.status == 503) {
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
  this.save_();
  onDone(this.lastError_ == '');
};

/**
 * Display the list of hosts or error condition.
 *
 * @return {void} Nothing.
 */
remoting.HostList.prototype.display = function() {
  this.table_.innerText = '';
  this.errorMsg_.innerText = '';
  this.hostTableEntries_ = [];

  var noHostsRegistered = (this.hosts_.length == 0);
  this.table_.hidden = noHostsRegistered;
  this.noHosts_.hidden = !noHostsRegistered;

  for (var i = 0; i < this.hosts_.length; ++i) {
    /** @type {remoting.Host} */
    var host = this.hosts_[i];
    // Validate the entry to make sure it has all the fields we expect and is
    // not the local host (which is displayed separately). NB: if the host has
    // never sent a heartbeat, then there will be no jabberId.
    if (host.hostName && host.hostId && host.status && host.publicKey &&
        (!this.localHost_ || host.hostId != this.localHost_.hostId)) {
      var hostTableEntry = new remoting.HostTableEntry(
          host, this.webappMajorVersion_,
          this.renameHost_.bind(this), this.deleteHost_.bind(this));
      hostTableEntry.createDom();
      this.hostTableEntries_[i] = hostTableEntry;
      this.table_.appendChild(hostTableEntry.tableRow);
    }
  }

  if (this.lastError_ != '') {
    l10n.localizeElementFromTag(this.errorMsg_, this.lastError_);
    if (this.lastError_ == remoting.Error.AUTHENTICATION_FAILED) {
      l10n.localizeElementFromTag(this.errorButton_,
                                  /*i18n-content*/'SIGN_IN_BUTTON');
    } else {
      l10n.localizeElementFromTag(this.errorButton_,
                                  /*i18n-content*/'RETRY');
    }
  }
  this.errorMsg_.parentNode.hidden = (this.lastError_ == '');

  var state = this.localHostState_;
  var enabled = (state == remoting.HostController.State.STARTING) ||
      (state == remoting.HostController.State.STARTED);
  var supported = (state != remoting.HostController.State.NOT_IMPLEMENTED);
  remoting.updateModalUi(enabled ? 'enabled' : 'disabled', 'data-daemon-state');
  document.getElementById('daemon-control').hidden = !supported;
  var element = document.getElementById('host-list-empty-hosting-supported');
  element.hidden = !supported;
  element = document.getElementById('host-list-empty-hosting-unsupported');
  element.hidden = supported;
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
 * Prepare a host for renaming by replacing its name with an edit box.
 * @param {remoting.HostTableEntry} hostTableEntry The host to be renamed.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.renameHost_ = function(hostTableEntry) {
  for (var i = 0; i < this.hosts_.length; ++i) {
    if (this.hosts_[i].hostId == hostTableEntry.host.hostId) {
      this.hosts_[i].hostName = hostTableEntry.host.hostName;
      break;
    }
  }
  this.save_();

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
          remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts/' +
          hostTableEntry.host.hostId,
          function(xhr) {},
          JSON.stringify(newHostDetails),
          headers);
    } else {
      console.error('Could not rename host. Authentication failure.');
    }
  }
  remoting.identity.callWithToken(renameHost, remoting.showErrorMessage);
};

/**
 * Unregister a host.
 * @param {string} hostId The id of the host to be removed.
 * @return {void} Nothing.
 */
remoting.HostList.unregisterHostById = function(hostId) {
  /** @param {string} token The OAuth2 token. */
  var deleteHost = function(token) {
    var headers = { 'Authorization': 'OAuth ' + token };
    remoting.xhr.remove(
        remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts/' + hostId,
        function() {}, '', headers);
  }
  remoting.identity.callWithToken(deleteHost, remoting.showErrorMessage);
};

/**
 * Set tool-tips for the 'connect' action. We can't just set this on the
 * parent element because the button has no tool-tip, and therefore would
 * inherit connectStr.
 *
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.setTooltips_ = function() {
  var connectStr = '';
  if (this.localHost_) {
    chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_CONNECT',
                           this.localHost_.hostName);
  }
  document.getElementById('this-host-name').title = connectStr;
  document.getElementById('this-host-icon').title = connectStr;
};

/**
 * Set the state of the local host and localHostId if any.
 *
 * @param {remoting.HostController.State} state State of the local host.
 * @param {string?} hostId ID of the local host, or null.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.setLocalHostStateAndId = function(state, hostId) {
  this.localHostState_ = state;
  this.setLocalHost_(hostId ? this.getHostForId(hostId) : null);
}

/**
 * Set the host object that corresponds to the local computer, if any.
 *
 * @param {remoting.Host?} host The host, or null if not registered.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.setLocalHost_ = function(host) {
  this.localHost_ = host;
  this.setTooltips_();
  /** @type {remoting.HostList} */
  var that = this;
  if (host) {
    /** @param {remoting.HostTableEntry} host */
    var renameHost = function(host) {
      that.renameHost_(host);
      that.setTooltips_();
    };
    if (!this.localHostTableEntry_) {
      /** @type {remoting.HostTableEntry} @private */
      this.localHostTableEntry_ = new remoting.HostTableEntry(
          host, this.webappMajorVersion_, renameHost);
      this.localHostTableEntry_.init(
          document.getElementById('this-host-connect'),
          document.getElementById('this-host-warning'),
          document.getElementById('this-host-name'),
          document.getElementById('this-host-rename'));
    } else {
      // TODO(jamiewalch): This is hack to prevent multiple click handlers being
      // registered for the same DOM elements if this method is called more than
      // once. A better solution would be to let HostTable create the daemon row
      // like it creates the rows for non-local hosts.
      this.localHostTableEntry_.host = host;
    }
  } else {
    this.localHostTableEntry_ = null;
  }
}

/**
 * Called by the HostControlled after the local host has been started.
 *
 * @param {string} hostName Host name.
 * @param {string} hostId ID of the local host.
 * @param {string} publicKey Public key.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.onLocalHostStarted = function(
    hostName, hostId, publicKey) {
  // Create a dummy remoting.Host instance to represent the local host.
  // Refreshing the list is no good in general, because the directory
  // information won't be in sync for several seconds. We don't know the
  // host JID, but it can be missing from the cache with no ill effects.
  // It will be refreshed if the user tries to connect to the local host,
  // and we hope that the directory will have been updated by that point.
  var localHost = new remoting.Host();
  localHost.hostName = hostName;
  localHost.hostId = hostId;
  localHost.publicKey = publicKey;
  localHost.status = 'ONLINE';
  this.hosts_.push(localHost);
  this.save_();
  this.setLocalHost_(localHost);
};

/**
 * Called when the user clicks the button next to the error message. The action
 * depends on the error.
 *
 * @private
 */
remoting.HostList.prototype.onErrorClick_ = function() {
  if (this.lastError_ == remoting.Error.AUTHENTICATION_FAILED) {
    remoting.oauth2.doAuthRedirect();
  } else {
    this.lastError_ = '';
    this.display();
    this.refresh(remoting.updateLocalHostState);
  }
};

/**
 * Save the host list to local storage.
 */
remoting.HostList.prototype.save_ = function() {
  var items = {};
  items[remoting.HostList.HOSTS_KEY] = JSON.stringify(this.hosts_);
  chrome.storage.local.set(items);
};

/**
 * Key name under which Me2Me hosts are cached.
 */
remoting.HostList.HOSTS_KEY = 'me2me-cached-hosts';

/** @type {remoting.HostList} */
remoting.hostList = null;
