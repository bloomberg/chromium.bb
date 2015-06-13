// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of network state
 * properties in a list in the format:
 *    Key1.........Value1
 *    KeyTwo.......ValueTwo
 * TODO(stevenjb): Translate the keys and (where appropriate) values.
 */
(function() {

Polymer({
  is: 'cr-network-property-list',

  properties: {
    /**
     * The network state containing the properties to display.
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null
    },

    /**
     * Fields to dispaly.
     * @type {!Array<string>}
     */
    fields: {
      type: Array,
      value: function() { return []; }
    },
  },

  /**
   * @param {string} key The property key.
   * @return {string} The text to display for the property label.
   * @private
   */
  getPropertyLabel_: function(key) {
    // TODO(stevenjb): Localize.
    return key;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @param {string} key The property key.
   * @return {boolean} Whether or not the property exists in state.
   * @private
   */
  hasPropertyValue_: function(state, key) {
    if (!state)
      return false;
    var value = this.get(key, state);
    return (value !== undefined && value !== '');
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @param {string} key The property key.
   * @return {string} The text to display for the property value.
   * @private
   */
  getPropertyValue_: function(state, key) {
    if (!state)
      return '';
    var value = CrOnc.getActiveValue(state, key);
    if (value === undefined)
      return '';
    // TODO(stevenjb): Localize.
    return value;
  },
});
})();
