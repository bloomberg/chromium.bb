// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a collapsable list of networks.
 */

/**
 * Polymer class definition for 'cr-network-list'.
 */
Polymer({
  is: 'cr-network-list',

  properties: {
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

    /** True if action buttons should be shown for the itmes. */
    showButtons: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * Reflects the iron-list selecteditem property.
     * @type {!CrNetworkList.CrNetworkListItemType}
     */
    selectedItem: {
      type: Object,
      observer: 'selectedItemChanged_',
    }
  },

  behaviors: [CrScrollableBehavior],

  observers: ['listChanged_(networks, customItems)'],

  /** @private */
  listChanged_: function() {
    this.updateScrollableContents();
  },

  /**
   * Returns a combined list of networks and custom items.
   * @return {!Array<!CrNetworkList.CrNetworkListItemType>}
   * @private
   */
  getItems_: function() {
    var customItems = this.customItems.slice();
    // Flag the first custom item with isFirstCustomItem = true.
    if (customItems.length > 0)
      customItems[0].isFirstCustomItem = true;
    return this.networks.concat(customItems);
  },

  /**
   * Use iron-list selection (which is not the same as focus) to trigger
   * tap (requires selection-enabled) or keyboard selection.
   * @private
   */
  selectedItemChanged_: function() {
    if (this.selectedItem)
      this.onItemAction_(this.selectedItem);
  },

  /**
   * @param {!CrNetworkList.CrNetworkListItemType} item
   * @private
   */
  onItemAction_: function(item) {
    if (item.hasOwnProperty('customItemName'))
      this.fire('custom-item-selected', item);
    else
      this.fire('selected', item);
  },
});
