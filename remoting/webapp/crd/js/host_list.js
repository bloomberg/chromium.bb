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
 * @param {HTMLElement} loadingIndicator The HTML <span> to update while the
 *     host list is being loaded. The first element of this span should be
 *     the reload button.
 * @param {function(!remoting.Error)} onError Function to call when an error
 *     occurs.
 */
remoting.HostList = function(table, noHosts, errorMsg, errorButton,
                             loadingIndicator, onError) {
  /** @private {Element} */
  this.table_ = table;
  /**
   * TODO(jamiewalch): This should be doable using CSS's sibling selector,
   * but it doesn't work right now (crbug.com/135050).
   * @private {Element}
   */
  this.noHosts_ = noHosts;
  /** @private {Element} */
  this.errorMsg_ = errorMsg;
  /** @private {Element} */
  this.errorButton_ = errorButton;
  /** @private {HTMLElement} */
  this.loadingIndicator_ = loadingIndicator;
  this.onError_ = onError;

  /** @private {Array<remoting.HostTableEntry>} */
  this.hostTableEntries_ = [];
  /** @private {Array<remoting.Host>} */
  this.hosts_ = [];
  /** @private {!remoting.Error} */
  this.lastError_ = remoting.Error.none();
  /** @private {remoting.LocalHostSection} */
  this.localHostSection_ = new remoting.LocalHostSection(
      /** @type {HTMLElement} */ (document.querySelector('.daemon-control')),
      new remoting.LocalHostSection.Controller(
          this,
          new remoting.HostSetupDialog(remoting.hostController, onError)));

  /** @private {number} */
  this.webappMajorVersion_ = parseInt(chrome.runtime.getManifest().version, 10);

  this.errorButton_.addEventListener('click',
                                     this.onErrorClick_.bind(this),
                                     false);
  var reloadButton = this.loadingIndicator_.firstElementChild;
  /** @type {remoting.HostList} */
  var that = this;
  /** @param {Event} event */
  function refresh(event) {
    event.preventDefault();
    that.refresh(that.display.bind(that));
  }
  reloadButton.addEventListener('click', refresh, false);
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
  /** @param {Object<string>} items */
  var storeHostList = function(items) {
    if (items[remoting.HostList.HOSTS_KEY]) {
      var cached = base.jsonParseSafe(items[remoting.HostList.HOSTS_KEY]);
      if (cached) {
        that.hosts_ = /** @type {Array<remoting.Host>} */ (cached);
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
 * Get the host id corresponding to the specified host name.
 *
 * @param {string} hostName The name of the host.
 * @return {string?} The host id, if a host with the given name exists.
 */
remoting.HostList.prototype.getHostIdForName = function(hostName) {
  for (var i = 0; i < this.hosts_.length; ++i) {
    if (this.hosts_[i].hostName == hostName) {
      return this.hosts_[i].hostId;
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
  this.loadingIndicator_.classList.add('loading');
  /** @type {remoting.HostList} */
  var that = this;
  /** @param {!remoting.Error} error */
  var onError = function(error) {
    that.lastError_ = error;
    onDone(false);
  };
  remoting.hostListApi.get(this.onHostListResponse_.bind(this, onDone),
                           onError);
};

/**
 * Handle the results of the host list request.  A success response will
 * include a JSON-encoded list of host descriptions, which we display if we're
 * able to successfully parse it.
 *
 * @param {function(boolean):void} onDone The callback passed to |refresh|.
 * @param {Array<remoting.Host>} hosts The list of hosts for the user.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.onHostListResponse_ = function(onDone, hosts) {
  this.lastError_ = remoting.Error.none();
  this.hosts_ = hosts;
  this.sortHosts_();
  this.save_();
  this.loadingIndicator_.classList.remove('loading');
  onDone(true);
};

/**
 * Sort the internal list of hosts.
 *
 * @suppress {reportUnknownTypes}
 * @return {void} Nothing.
 */
remoting.HostList.prototype.sortHosts_ = function() {
  /**
   * Sort hosts, first by ONLINE/OFFLINE status and then by host-name.
   *
   * @param {remoting.Host} a
   * @param {remoting.Host} b
   * @return {number}
   */
  var cmp = function(a, b) {
    if (a.status < b.status) {
      return 1;
    } else if (b.status < a.status) {
      return -1;
    } else if (a.hostName.toLocaleLowerCase() <
               b.hostName.toLocaleLowerCase()) {
      return -1;
    } else if (a.hostName.toLocaleLowerCase() >
               b.hostName.toLocaleLowerCase()) {
      return 1;
    }
    return 0;
  };

  this.hosts_ = this.hosts_.sort(cmp);
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

  if (!this.lastError_.isNone()) {
    l10n.localizeElementFromTag(this.errorMsg_, this.lastError_.getTag());
    if (this.lastError_.hasTag(remoting.Error.Tag.AUTHENTICATION_FAILED)) {
      l10n.localizeElementFromTag(this.errorButton_,
                                  /*i18n-content*/'SIGN_IN_BUTTON');
    } else {
      l10n.localizeElementFromTag(this.errorButton_,
                                  /*i18n-content*/'RETRY');
    }
  } else {
    for (var i = 0; i < this.hosts_.length; ++i) {
      /** @type {remoting.Host} */
      var host = this.hosts_[i];
      // Validate the entry to make sure it has all the fields we expect and is
      // not the local host (which is displayed separately). NB: if the host has
      // never sent a heartbeat, then there will be no jabberId.
      if (host.hostName && host.hostId && host.status && host.publicKey &&
          (host.hostId != this.localHostSection_.getHostId())) {
        var hostTableEntry = new remoting.HostTableEntry(
            this.webappMajorVersion_,
            remoting.connectMe2Me,
            this.renameHost.bind(this),
            this.deleteHost_.bind(this));
        hostTableEntry.setHost(host);
        this.hostTableEntries_[i] = hostTableEntry;
        this.table_.appendChild(hostTableEntry.element());
      }
    }
  }

  this.errorMsg_.parentNode.hidden = this.lastError_.isNone();
  if (noHostsRegistered) {
    this.showHostListEmptyMessage_(this.localHostSection_.canChangeState());
  }
};

/**
 * Displays a message to the user when the host list is empty.
 *
 * @param {boolean} hostingSupported
 * @return {void}
 * @private
 */
remoting.HostList.prototype.showHostListEmptyMessage_ = function(
    hostingSupported) {
  var that = this;
  remoting.AppsV2Migration.hasHostsInV1App().then(
    /**
     * @param {remoting.MigrationSettings} previousIdentity
     * @this {remoting.HostList}
     */
    function(previousIdentity) {
      that.noHosts_.innerHTML = remoting.AppsV2Migration.buildMigrationTips(
          previousIdentity.email, previousIdentity.fullName);
    },
    function() {
      var buttonLabel = l10n.getTranslationOrError(
          /*i18n-content*/'HOME_DAEMON_START_BUTTON');
      if (hostingSupported) {
        that.noHosts_.innerText = l10n.getTranslationOrError(
            /*i18n-content*/'HOST_LIST_EMPTY_HOSTING_SUPPORTED',
            [buttonLabel]);
      } else {
        that.noHosts_.innerText = l10n.getTranslationOrError(
            /*i18n-content*/'HOST_LIST_EMPTY_HOSTING_UNSUPPORTED',
            [buttonLabel]);
      }
    }
  );
};

/**
 * Remove a host from the list, and deregister it.
 * @param {remoting.HostTableEntry} hostTableEntry The host to be removed.
 * @return {void} Nothing.
 * @private
 */
remoting.HostList.prototype.deleteHost_ = function(hostTableEntry) {
  this.table_.removeChild(hostTableEntry.element());
  var index = this.hostTableEntries_.indexOf(hostTableEntry);
  if (index != -1) {
    this.hostTableEntries_.splice(index, 1);
  }
  remoting.hostListApi.remove(hostTableEntry.host.hostId, base.doNothing,
                              this.onError_);
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
  this.save_();

  remoting.hostListApi.put(hostTableEntry.host.hostId,
                           hostTableEntry.host.hostName,
                           hostTableEntry.host.publicKey,
                           function() {},
                           this.onError_);
};

/**
 * Unregister a host.
 * @param {string} hostId The id of the host to be removed.
 * @param {function(void)=} opt_onDone
 * @return {void} Nothing.
 */
remoting.HostList.prototype.unregisterHostById = function(hostId, opt_onDone) {
  var that = this;
  var onDone = opt_onDone || base.doNothing;

  var host = this.getHostForId(hostId);
  if (!host) {
    console.log('Skipping host un-registration as the host is not registered ' +
                'under the current account');
    onDone();
    return;
  }

  var onRemoved = function() {
    that.refresh(function() {
      that.display();
      onDone();
    });
  };
  remoting.hostListApi.remove(hostId, onRemoved, this.onError_);
};

/**
 * Set the state of the local host and localHostId if any.
 *
 * @param {remoting.HostController.State} state State of the local host.
 * @param {string?} hostId ID of the local host, or null.
 * @return {void} Nothing.
 */
remoting.HostList.prototype.setLocalHostStateAndId = function(state, hostId) {
  var host = hostId ? this.getHostForId(hostId) : null;
  this.localHostSection_.setModel(host, state, !this.lastError_.isNone());
};

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
  // Provide a version number to avoid warning about this dummy host being
  // out-of-date.
  localHost.hostVersion = String(this.webappMajorVersion_) + ".x"
  localHost.hostId = hostId;
  localHost.publicKey = publicKey;
  localHost.status = 'ONLINE';
  this.hosts_.push(localHost);
  this.save_();
  this.localHostSection_.setModel(localHost,
                                  remoting.HostController.State.STARTED,
                                  !this.lastError_.isNone());
};

/**
 * Called when the user clicks the button next to the error message. The action
 * depends on the error.
 *
 * @private
 */
remoting.HostList.prototype.onErrorClick_ = function() {
  if (this.lastError_.hasTag(remoting.Error.Tag.AUTHENTICATION_FAILED)) {
    remoting.handleAuthFailureAndRelaunch();
  } else {
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
  if (this.hosts_.length !== 0) {
    remoting.AppsV2Migration.saveUserInfo();
  }
};

/**
 * Key name under which Me2Me hosts are cached.
 */
remoting.HostList.HOSTS_KEY = 'me2me-cached-hosts';

/** @type {remoting.HostList} */
remoting.hostList = null;
