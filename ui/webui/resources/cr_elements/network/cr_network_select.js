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
      }
    },

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

  /** @override */
  attached: function() {
    this.networkListChangedListener_ = this.refreshNetworks.bind(this);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
        this.networkListChangedListener_);

    this.deviceStateListChangedListener_ = this.refreshNetworks.bind(this);
    chrome.networkingPrivate.onDeviceStateListChanged.addListener(
        this.deviceStateListChangedListener_);

    this.refreshNetworks();
    chrome.networkingPrivate.requestNetworkScan();
  },

  /** @override */
  detached: function() {
    chrome.networkingPrivate.onNetworkListChanged.removeListener(
        this.networkListChangedListener_);
    chrome.networkingPrivate.onDeviceStateListChanged.removeListener(
        this.deviceStateListChangedListener_);
  },

  /**
   * Request the list of visible networks. May be called externally to force a
   * refresh and list update (e.g. when the element is shown).
   * @private
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
  },

  /**
   * Event triggered when a cr-network-list-item is selected.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onNetworkListItemSelected_: function(event) {
    var state = event.detail;

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
});
