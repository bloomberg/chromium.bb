// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about a network
 * in a list based on ONC state properties.
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
     * @type {!OncMojo.NetworkStateProperties|undefined}
     */
    networkState: {
      type: Object,
      observer: 'networkStateChanged_',
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

    /**
     * Expose the itemName so it can be used as a label for a11y.  It will be
     * added as an attribute on this top-level cr-network-list-item, and can
     * be used by any sub-element which applies it.
     */
    ariaLabel: {
      type: String,
      notify: true,
      reflectToAttribute: true,
      computed: 'getItemName_(item)',
    },

    /**
     * The cached ConnectionState for the network.
     * @type {!chromeos.networkConfig.mojom.ConnectionStateType|undefined}
     */
    connectionState_: Number,

    /** Whether to show technology badge on mobile network icon. */
    showTechnologyBadge: {type: Boolean, value: true},
  },

  behaviors: [CrPolicyNetworkBehavior],

  /** @override */
  attached: function() {
    this.listen(this, 'keydown', 'onKeydown_');
  },

  /** @override */
  detached: function() {
    this.unlisten(this, 'keydown', 'onKeydown_');
  },

  /** @private */
  itemChanged_: function() {
    if (this.item && !this.item.hasOwnProperty('customItemName')) {
      this.networkState =
          /** @type {!OncMojo.NetworkStateProperties} */ (this.item);
    } else if (this.networkState) {
      this.networkState = undefined;
    }
  },

  /** @private */
  networkStateChanged_: function() {
    if (!this.networkState) {
      return;
    }
    const connectionState = this.networkState.connectionState;
    if (connectionState == this.connectionState_) {
      return;
    }
    this.connectionState_ = connectionState;
    this.fire('network-connect-changed', this.networkState);
  },

  /**
   * This gets called for network items and custom items.
   * @return {string}
   * @private
   */
  getItemName_: function() {
    if (this.item.hasOwnProperty('customItemName')) {
      const item = /** @type {!CrNetworkList.CustomItemState} */ (this.item);
      let name = item.customItemName || '';
      if (CrOncStrings.hasOwnProperty(item.customItemName)) {
        name = CrOncStrings[item.customItemName];
      }
      return name;
    }
    return OncMojo.getNetworkDisplayName(
        /** @type {!OncMojo.NetworkStateProperties} */ (this.item));
  },

  /**
   * @return {boolean}
   * @private
   */
  isStateTextVisible_: function() {
    return !!this.networkState && !!this.getNetworkStateText_();
  },

  /**
   * This only gets called for network items once networkState is set.
   * @return {string}
   * @private
   */
  getNetworkStateText_: function() {
    const mojom = chromeos.networkConfig.mojom;
    if (!this.networkState) {
      return '';
    }
    const connectionState = this.networkState.connectionState;
    if (this.networkState.type == mojom.NetworkType.kCellular &&
        this.networkState.cellular.scanning) {
      // TODO(khorimoto): Add and sim locked and possibly initializing states to
      // CellularStateProperties.
      return CrOncStrings.networkListItemScanning;
    }
    if (OncMojo.connectionStateIsConnected(connectionState)) {
      // TODO(khorimoto): Consider differentiating between Portal, Connected,
      // and Online.
      return CrOncStrings.networkListItemConnected;
    }
    if (connectionState == mojom.ConnectionStateType.kConnecting) {
      return CrOncStrings.networkListItemConnecting;
    }
    return '';
  },

  /**
   * @param {!OncMojo.NetworkStateProperties|undefined} networkState
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
    if (!this.networkState) {
      return false;
    }
    return OncMojo.connectionStateIsConnected(
        this.networkState.connectionState);
  },

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydown_: function(event) {
    // The only key event handled by this element is pressing Enter when the
    // subpage arrow is focused.
    if (event.key != 'Enter' ||
        !this.isSubpageButtonVisible_(this.networkState, this.showButtons) ||
        this.$$('#subpage-button') != this.shadowRoot.activeElement) {
      return;
    }

    this.fireShowDetails_(event);

    // The default event for pressing Enter on a focused button is to simulate a
    // click on the button. Prevent this action, since it would navigate a
    // second time to the details page and cause an unnecessary entry to be
    // added to the back stack. See https://crbug.com/736963.
    event.preventDefault();
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onSubpageArrowClick_: function(event) {
    this.fireShowDetails_(event);
  },

  /**
   * Fires a 'show-details' event with |this.networkState| as the details.
   * @param {!Event} event
   * @private
   */
  fireShowDetails_: function(event) {
    assert(this.networkState);
    this.fire('show-detail', this.networkState);
    event.stopPropagation();
  },
});
