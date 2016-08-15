// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about a network
 * in a list or summary based on ONC state properties.
 */
(function() {
'use strict';

/**
 * TODO(stevenjb): Replace getText with a proper localization function that
 * handles string substitution.
 * Performs argument substitution, replacing $1, $2, etc in 'text' with
 * corresponding entries in |args|.
 * @param {string} text The string to perform the substitution on.
 * @param {?Array<string>=} opt_args The arguments to replace $1, $2, etc with.
 */
function getText(text, opt_args) {
  var res;
  if (loadTimeData && loadTimeData.data_)
    res = loadTimeData.getString(text) || text;
  else
    res = text;
  if (!opt_args)
    return res;
  for (let i = 0; i < opt_args.length; ++i) {
    let key = '$' + (i + 1);
    res = res.replace(key, opt_args[i]);
  }
  return res;
}

/**
 * Returns the appropriate connection state text.
 * @param {string} state The connection state.
 * @param {string} name The name of the network.
 */
function getConnectionStateText(state, name) {
  if (state == CrOnc.ConnectionState.CONNECTED)
    return name;
  if (state == CrOnc.ConnectionState.CONNECTING)
    return getText('networkConnecting', [name]);
  if (state == CrOnc.ConnectionState.NOT_CONNECTED)
    return getText('networkNotConnected');
  return getText(state);
};

/**
 * Returns the name to display for |network|.
 * @param {?CrOnc.NetworkStateProperties} network
 */
function getNetworkName(network) {
  var name = network.Name;
  if (!name)
    return getText('OncType' + network.Type);
  if (network.Type == 'VPN' && network.VPN &&
      network.VPN.Type == 'ThirdPartyVPN' && network.VPN.ThirdPartyVPN) {
    var providerName = network.VPN.ThirdPartyVPN.ProviderName;
    if (providerName)
      return getText('vpnNameTemplate', [providerName, name]);
  }
  return name;
}

/**
 * Polymer class definition for 'cr-network-list-network-item'.
 */
Polymer({
  is: 'cr-network-list-network-item',

  properties: {
    /**
     * The ONC data properties used to display the list item.
     *
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null,
      observer: 'networkStateChanged_',
    },

    /**
     * Determines how the list item will be displayed:
     *  True - Displays the network icon (with strength) and name
     *  False - The element is a stand-alone item (e.g. part of a summary)
     *      and displays the name of the network type plus the network name
     *      and connection state.
     */
    listItem: {
      type: Boolean,
      value: false,
      observer: 'networkStateChanged_',
    },

    /**
     * Whether to show buttons.
     */
    showButtons: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Polymer networkState changed method. Updates the element based on the
   * network state.
   */
  networkStateChanged_: function() {
    if (!this.networkState)
      return;
    var network = this.networkState;
    var isDisconnected =
        network.ConnectionState == CrOnc.ConnectionState.NOT_CONNECTED;

    if (network.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
      this.fire("network-connected", this.networkState);
    }

    var name = getNetworkName(network);
    if (this.listItem) {
      this.$.itemName.textContent = name;
      this.$.itemName.classList.toggle('connected', !isDisconnected);
      if (!isDisconnected) {
        this.$.networkStateText.textContent =
            getText('networkListItemConnected');
        this.$.networkStateText.classList.toggle('connected', true);
      }
      return;
    }

    if (network.Name && network.ConnectionState) {
      this.$.itemName.textContent = getText('OncType' + network.Type);
      this.$.itemName.classList.toggle('connected', false);
      this.$.networkStateText.textContent =
          getConnectionStateText(network.ConnectionState, name);
      this.$.networkStateText.classList.toggle('connected', false);
      return;
    }

    this.$.itemName.textContent = getText('OncType' + network.Type);
    this.$.itemName.classList.toggle('connected', false);
    this.$.networkStateText.textContent = getText('networkDisabled');
    this.$.networkStateText.classList.toggle('connected', false);
    if (network.Type == CrOnc.Type.CELLULAR) {
      if (!network.GUID)
        this.$.networkStateText.textContent = getText('networkDisabled');
    }
  },

  /**
   * Fires a 'show-details' event with |this.networkState| as the details.
   * @param {Event} event
   * @private
   */
  fireShowDetails_: function(event) {
    this.fire('show-detail', this.networkState);
    event.stopPropagation();
  },

  /** @private */
  isStateVisible_(networkState, listItem) {
    return !this.listItem ||
        networkState.ConnectionState != CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /** @private */
  isSettingsButtonVisible_: function(showButtons, listItem) {
    return showButtons && this.listItem;
  },
});
})();
