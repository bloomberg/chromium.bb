// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping cr-network-list including the
 * networkingPrivate calls to populate it.
 */

Polymer({
  is: 'cr-network-select',

  properties: {
    /**
     * Show all buttons in list items.
     */
    showButtons: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * The list of custom items to display after the list of networks.
     * See CrNetworkList for details.
     * @type {!Array<CrNetworkList.CustomItemState>}
     */
    customItems: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Whether to handle "item-selected" for network items.
     * If this property is false, "network-item-selected" event is fired
     * carrying CrOnc.NetworkStateProperties as event detail.
     * @type {Function}
     */
    handleNetworkItemSelected: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * List of all network state data for all visible networks.
     * @private {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList_: {
      type: Array,
      value: function() {
        return [];
      },
    },
  },

  focus: function() {
    this.$.neworkList.focus();
  },

  /**
   * Listener function for chrome.networkingPrivate.onNetworkListChanged event.
   * @type {function(!Array<string>)}
   * @private
   */
  networkListChangedListener_: function() {},

  /**
   * Listener function for chrome.networkingPrivate.onDeviceStateListChanged
   * event.
   * @type {function(!Array<string>)}
   * @private
   */
  deviceStateListChangedListener_: function() {},

  /** @private {number|null} */
  scanIntervalId_: null,

  /** @override */
  attached: function() {
    this.networkListChangedListener_ = this.refreshNetworks.bind(this);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
        this.networkListChangedListener_);

    this.deviceStateListChangedListener_ = this.refreshNetworks.bind(this);
    chrome.networkingPrivate.onDeviceStateListChanged.addListener(
        this.deviceStateListChangedListener_);

    this.refreshNetworks();

    /** @const */ var INTERVAL_MS = 10 * 1000;
    chrome.networkingPrivate.requestNetworkScan();
    this.scanIntervalId_ = window.setInterval(function() {
      chrome.networkingPrivate.requestNetworkScan();
    }.bind(this), INTERVAL_MS);
  },

  /** @override */
  detached: function() {
    if (this.scanIntervalId_ !== null)
      window.clearInterval(this.scanIntervalId_);
    chrome.networkingPrivate.onNetworkListChanged.removeListener(
        this.networkListChangedListener_);
    chrome.networkingPrivate.onDeviceStateListChanged.removeListener(
        this.deviceStateListChangedListener_);
  },

  /**
   * Request the list of visible networks. May be called externally to force a
   * refresh and list update (e.g. when the element is shown).
   */
  refreshNetworks: function() {
    var filter = {
      networkType: chrome.networkingPrivate.NetworkType.ALL,
      visible: true,
      configured: false
    };
    chrome.networkingPrivate.getNetworks(
        filter, this.getNetworksCallback_.bind(this));
  },

  /**
   * @param {!Array<!CrOnc.NetworkStateProperties>} states
   * @private
   */
  getNetworksCallback_: function(states) {
    this.networkStateList_ = states;
    var defaultState = (this.networkStateList_.length > 0 &&
                        this.networkStateList_[0].ConnectionState ==
                            CrOnc.ConnectionState.CONNECTED) ?
        this.networkStateList_[0] :
        null;
    this.defaultNetworkChanged_(defaultState);
  },

  /**
   * Event triggered when a cr-network-list-item is selected.
   * @param {!{target: HTMLElement, detail: !CrOnc.NetworkStateProperties}} e
   * @private
   */
  onNetworkListItemSelected_: function(e) {
    var state = e.detail;
    e.target.blur();

    if (!this.handleNetworkItemSelected) {
      this.fire('network-item-selected', state);
      return;
    }

    if (state.ConnectionState != CrOnc.ConnectionState.NOT_CONNECTED)
      return;

    chrome.networkingPrivate.startConnect(state.GUID, function() {
      var lastError = chrome.runtime.lastError;
      if (lastError && lastError != 'connecting')
        console.error('networkingPrivate.startConnect error: ' + lastError);
    });
  },

  /**
   * Event triggered when a cr-network-list-item becomes connected.
   * @param {!{target: HTMLElement, detail: !CrOnc.NetworkStateProperties}} e
   * @private
   */
  onNetworkConnected_: function(e) {
    this.defaultNetworkChanged_(e.detail);
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state
   * @private
   */
  defaultNetworkChanged_: function(state) {
    this.fire('default-network-changed', state);
  },
});
