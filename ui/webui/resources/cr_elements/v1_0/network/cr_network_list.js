// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a collapsable list of networks.
 */

(function() {

/**
 * Polymer class definition for 'cr-network-list'.
 * TODO(stevenjb): Update with iron-list(?) once implemented in Polymer 1.0.
 * @element cr-network-list
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
      observer: 'maxHeightChanged_'
    },

    /**
     * The list of network state properties for the items to display.
     *
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networks: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * True if the list is opened.
     */
    opened: {
      type: Boolean,
      value: true
    }
  },

  /**
   * Polymer maxHeight changed method.
   */
  maxHeightChanged_: function() {
    this.$.container.style.maxHeight = this.maxHeight + 'px';
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
   * @param {!{target: !NetworkListItem}} event
   * @private
   */
  onSelected_: function(event) {
    this.fire('selected', event.model.item);
  }
});
})();
