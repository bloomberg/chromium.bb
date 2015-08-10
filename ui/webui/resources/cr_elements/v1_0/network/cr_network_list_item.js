// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about a network
 * in a list or summary based on ONC state properties.
 */
(function() {

/**
 * TODO(stevenjb): Replace getText with a proper localization function that
 * handles string substitution.
 * Performs argument substitution, replacing $1, $2, etc in 'text' with
 * corresponding entries in |args|.
 * @param {string} text The string to perform the substitution on.
 * @param {?Array<string>} args The arguments to replace $1, $2, etc with.
 */
function getText(text, args) {
  var res;
  if (loadTimeData && loadTimeData.data_)
    res = loadTimeData.getString(text) || text;
  else
    res = text;
  if (!args)
    return res;
  for (var i = 0; i < args.length; ++i) {
    var key = '$' + (i + 1);
    res = res.replace(key, args[i]);
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
    return getText('networkConnected', [name]);
  if (state == CrOnc.ConnectionState.CONNECTING)
    return getText('networkConnecting', [name]);
  if (state == CrOnc.ConnectionState.NOT_CONNECTED)
    return getText('networkNotConnected');
  return getText(state);
};

/**
 * Polymer class definition for 'cr-network-list-item'.
 * @element cr-network-list-item
 */
Polymer({
  is: 'cr-network-list-item',

  properties: {
    /**
     * The ONC data properties used to display the list item.
     *
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null,
      observer: 'networkStateChanged_'
    },

    /**
     * If true, the element is part of a list of networks and only displays
     * the network icon and name. Otherwise the element is assumed to be a
     * stand-alone item (e.g. as part of a summary) and displays the name
     * of the network type plus the network name and connection state.
     */
    isListItem: {
      type: Boolean,
      value: false,
      observer: 'networkStateChanged_'
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
    if (this.isListItem) {
      var name = network.Name || getText('OncType' + network.Type);
      this.$.networkName.textContent = name;
      this.$.networkName.classList.toggle('connected', !isDisconnected);
      return;
    }
    if (network.Name && network.ConnectionState) {
      this.$.networkName.textContent = getText('OncType' + network.Type);
      this.$.networkName.classList.toggle('connected', false);
      this.$.networkStateText.textContent =
          getConnectionStateText(network.ConnectionState, network.Name);
      this.$.networkStateText.classList.toggle('connected', !isDisconnected);
      return;
    }
    this.$.networkName.textContent = getText('OncType' + network.Type);
    this.$.networkName.classList.toggle('connected', false);
    this.$.networkStateText.textContent = getText('networkDisabled');
    this.$.networkStateText.classList.toggle('connected', false);

    if (network.Type == CrOnc.Type.CELLULAR) {
      if (!network.GUID)
        this.$.networkStateText.textContent = getText('networkDisabled');
    }
  },

  /**
   * @param {string} isListItem The value of this.isListItem.
   * @return {string} The class name based on isListItem.
   * @private
   */
  isListItemClass_: function(isListItem) {
    return isListItem ? 'list-item' : '';
  }
});
})();
