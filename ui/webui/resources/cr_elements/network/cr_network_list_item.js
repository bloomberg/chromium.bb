// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about a network
 * in a list or summary based on ONC state properties.
 */

Polymer({
  is: 'cr-network-list-item',

  properties: {
    /** @type {!CrNetworkList.CrNetworkListItemType|undefined} */
    item: {
      type: Object,
      observer: 'itemChanged_',
    },

    /**
     * The ONC data properties used to display the list item.
     * @type {!CrOnc.NetworkStateProperties|undefined}
     */
    networkState: {
      type: Object,
      observer: 'networkStateChanged_',
    },

    /**
     * Determines how the list item will be displayed:
     *  True - Displays the network icon (with strength) and name
     *  False - The element is a stand-alone item (e.g. part of a summary)
     *      and displays the name of the network type plus the network name
     *      and connection state.
     */
    isListItem: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** Whether to show any buttons for network items. Defaults to false. */
    showButtons: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /**
     * Reflect the element's tabindex attribute to a property so that embedded
     * elements (e.g. the show subpage button) can become keyboard focusable
     * when this element has keyboard focus.
     */
    tabindex: {
      type: Number,
      value: -1,
      reflectToAttribute: true,
    },

    /** Expose the itemName so it can be used as a label for a11y.  */
    itemName: {
      type: String,
      notify: true,
      computed: 'getItemName_(item, isListItem)',
    }
  },

  behaviors: [CrPolicyNetworkBehavior],

  /** @private */
  itemChanged_: function() {
    if (this.item && !this.item.hasOwnProperty('customItemName')) {
      this.networkState =
          /** @type {!CrOnc.NetworkStateProperties} */ (this.item);
    } else if (this.networkState) {
      this.networkState = undefined;
    }
  },

  /** @private */
  networkStateChanged_: function() {
    if (this.networkState &&
        this.networkState.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
      this.fire('network-connected', this.networkState);
    }
  },

  /**
   * This gets called for network items and custom items.
   * @return {string}
   * @private
   */
  getItemName_: function() {
    if (this.item.hasOwnProperty('customItemName')) {
      var item = /** @type {!CrNetworkList.CustomItemState} */ (this.item);
      var name = item.customItemName || '';
      if (CrOncStrings.hasOwnProperty(item.customItemName))
        name = CrOncStrings[item.customItemName];
      return name;
    }
    var network = /** @type {!CrOnc.NetworkStateProperties} */ (this.item);
    if (this.isListItem)
      return CrOnc.getNetworkName(network);
    return CrOncStrings['OncType' + network.Type];
  },

  /**
   * @return {boolean}
   * @private
   */
  isStateTextVisible_: function() {
    return !!this.networkState &&
        (!this.isListItem || (this.networkState.ConnectionState !=
                              CrOnc.ConnectionState.NOT_CONNECTED));
  },

  /**
   * @return {boolean}
   * @private
   */
  isStateTextConnected_: function() {
    return this.isListItem && this.isConnected_();
  },

  /**
   * This only gets called for network items once networkState is set.
   * @return {string}
   * @private
   */
  getNetworkStateText_: function() {
    if (!this.isStateTextVisible_())
      return '';
    var network = this.networkState;
    var state = network.ConnectionState;
    var name = CrOnc.getNetworkName(network);
    if (this.isListItem) {
      if (state == CrOnc.ConnectionState.CONNECTED)
        return CrOncStrings.networkListItemConnected;
      if (state == CrOnc.ConnectionState.CONNECTING)
        return CrOncStrings.networkListItemConnecting;
      return '';
    }
    if (name && state)
      return this.getConnectionStateText_(state, name);
    return CrOncStrings.networkDisabled;
  },

  /**
   * Returns the appropriate connection state text.
   * @param {CrOnc.ConnectionState} state The connection state.
   * @param {string} name The name of the network.
   * @return {string}
   * @private
   */
  getConnectionStateText_: function(state, name) {
    switch (state) {
      case CrOnc.ConnectionState.CONNECTED:
        return name;
      case CrOnc.ConnectionState.CONNECTING:
        if (name)
          return CrOncStrings.networkListItemConnectingTo.replace('$1', name);
        return CrOncStrings.networkListItemConnecting;
      case CrOnc.ConnectionState.NOT_CONNECTED:
        return CrOncStrings.networkListItemNotConnected;
    }
    assertNotReached();
    return state;
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} networkState
   * @param {boolean} showButtons
   * @return {boolean}
   * @private
   */
  isSubpageButtonVisible_: function(networkState, showButtons) {
    return !!networkState && showButtons;
  },

  /**
   * @return {boolean}
   * @private
   */
  isConnected_: function() {
    return !!this.networkState &&
        this.networkState.ConnectionState == CrOnc.ConnectionState.CONNECTED;
  },

  /**
   * Fires a 'show-details' event with |this.networkState| as the details.
   * @param {Event} event
   * @private
   */
  fireShowDetails_: function(event) {
    assert(this.networkState);
    this.fire('show-detail', this.networkState);
    event.stopPropagation();
  },
});
