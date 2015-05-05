// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ONC network configuration support class. Wraps a dictionary
 * object containing ONC managed or unmanaged dictionaries. Also provides
 * special accessors for ONC properties. Used by consumers of the
 * chrome.networkingPrivate API. See components/onc/docs/onc_spec.html.
 */
Polymer('cr-onc-data', {
  publish: {
    /**
     * ONC configuration property dictionary, e.g. the result of a
     * chrome.networkingPrivate.getProperties() call.
     *
     * @attribute data
     * @type {chrome.networkingPrivate.NetworkStateProperties}
     * @default null
     */
    data: null,
  },

  /** @override */
  created: function() {
    this.data =
        /** @type {chrome.networkingPrivate.NetworkStateProperties} */({});
  },

  /**
   * TODO(stevenjb): Handle Managed property dictionaries.
   * @param {string} key The property key.
   * @return {?} The property value if it exists, otherwise undefined.
   */
  getProperty: function(key) {
    var data = this.data;
    while (true) {
      var index = key.indexOf('.');
      if (index < 0)
        break;
      var keyComponent = key.substr(0, index);
      if (!(keyComponent in data))
        return undefined;
      data = data[keyComponent];
      key = key.substr(index + 1);
    }
    return data[key];
  },

  /** @return {boolean} True if the network is connected. */
  connected: function() {
    return this.data.ConnectionState == CrOnc.ConnectionState.CONNECTED;
  },

  /** @return {boolean} True if the network is connecting. */
  connecting: function() {
    return this.data.ConnectionState == CrOnc.ConnectionState.CONNECTING;
  },

  /** @return {boolean} True if the network is disconnected. */
  disconnected: function() {
    return this.data.ConnectionState == CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /** @return {number} The signal strength of the network. */
  getStrength: function() {
    var type = this.data.Type;
    var key = type + '.SignalStrength';
    return this.getProperty(key) || 0;
  },

  /** @return {CrOnc.Security} The ONC security type. */
  getWiFiSecurity: function() {
    return this.getProperty('WiFi.Security') || CrOnc.Security.NONE;
  },

  /** @return {CrOnc.RoamingState} The ONC roaming state. */
  getCellularRoamingState: function() {
    return this.getProperty('Cellular.RoamingState') ||
        CrOnc.RoamingState.UNKNOWN;
  },

  /** @return {CrOnc.NetworkTechnology} The ONC network technology. */
  getCellularTechnology: function() {
    return this.getProperty('Cellular.NetworkTechnology') ||
        CrOnc.NetworkTechnology.UNKNOWN;
  }
});

// Temporary constructor method. TODO(stevenjb): Replace with proper
// construction after switching to Polymer 0.8.

var CrOncDataElement = {};

/**
 * Helper method to create and return a typed cr-onc-data Polymer element.
 * Sets the data property of the element to |state|.
 *
 * @param {!chrome.networkingPrivate.NetworkStateProperties} state The network
 *     state properties.
 * @return {!CrOncDataElement} A cr-onc-data element.
 */
CrOncDataElement.create = function(state) {
  var oncData = /** @type {!CrOncDataElement} */ (
      document.createElement('cr-onc-data'));
  oncData.data = state;
  return oncData;
};
