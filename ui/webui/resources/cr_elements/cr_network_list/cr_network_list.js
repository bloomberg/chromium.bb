// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a collapsable list of networks.
 */

/**
 * Polymer class definition for 'cr-network-list'.
 * @element cr-network-list
 */
Polymer('cr-network-list', {
  publish: {
    /**
     * The maximum height in pixels for the list.
     *
     * @attribute maxHeight
     * @type {number}
     * @default 1000
     */
    maxHeight: 1000,

    /**
     * The list of network state properties for the items to display.
     *
     * @attribute networks
     * @type {?Array<!CrOncDataElement>}
     * @default null
     */
    networks: null,

    /**
     * True if the list is opened.
     *
     * @attribute opened
     * @type {boolean}
     * @default false
     */
    opened: false,
  },

  /** @override */
  created: function() {
    this.networks = [];
  },

  /**
   * Polymer maxHeight changed method.
   */
  maxHeightChanged: function() {
    this.$.container.style.maxHeight = this.maxHeight + "px";
  },

  /**
   * Called when the cr-collapse element changes size (i.e. is opened).
   * @private
   */
  onResized_: function() {
    if (this.opened)
      this.$.networkList.updateSize();
  },

  /**
   * Event triggered when a list item is selected.
   * @param {!{detail: CrOncDataElement}} event
   * @private
   */
  onSelected_: function(event) {
    this.fire('selected', event.detail.item.networkState);
  }
});
