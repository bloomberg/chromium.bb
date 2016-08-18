// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a collapsable list of networks.
 */

/**
 * Polymer class definition for 'cr-network-list'.
 * TODO(stevenjb): Update with iron-list(?) once implemented in Polymer 1.0.
 */
Polymer({
  is: 'cr-network-list',

  properties: {
    /**
     * The maximum height in pixels for the list.
     */
    maxHeight: {
      type: Number,
      value: 1000,
      observer: 'maxHeightChanged_',
    },

    /**
     * The list of network state properties for the items to display.
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networks: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /**
     * The list of custom items to display after the list of networks.
     * @type {!Array<!CrNetworkList.CustomItemState>}
     */
    customItems: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /** True if the list is opened. */
    opened: {
      type: Boolean,
      value: true,
    },

    /** True if action buttons should be shown for the itmes. */
    showButtons: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** Whether to show separators between all items. */
    showSeparators: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /** @private */
  maxHeightChanged_: function() {
    this.$.container.style.maxHeight = this.maxHeight + 'px';
  },

  /**
   * Returns a combined list of networks and custom items.
   * @return {!Array<!CrNetworkList.CrNetworkListItemType>}
   * @private
   */
  getItems_: function() {
    let customItems = this.customItems.slice();
    // Flag the first custom item with isFirstCustomItem = true.
    if (!this.showSeparators && customItems.length > 0)
      customItems[0].isFirstCustomItem = true;
    return this.networks.concat(customItems);
  },

  /**
   * Event triggered when a list item is tapped.
   * @param {!{model: {item: !CrNetworkList.CrNetworkListItemType}}} event
   * @private
   */
  onTap_: function(event) {
    let item = event.model.item;
    if (item.hasOwnProperty('customItemName'))
      this.fire('custom-item-selected', event.model.item);
    else
      this.fire('selected', event.model.item);
  },
});
