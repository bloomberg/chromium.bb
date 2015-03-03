// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ONC network configuration support class. Wraps a dictionary
 * object containing ONC managed or unmanaged dictionaries. Also provides
 * special accessors for ONC properties. See cr-onc-types for ONC types,
 * e.g. CrOnc.NetworkConfigType. Used by consumers of the
 * chrome.networkingPrivate API. See components/onc/docs/onc_spec.html.
 */
Polymer('cr-onc-data', {
  publish: {
    /**
     * ONC configuration property dictionary, e.g. the result of a
     * chrome.networkingPrivate.getProperties() call.
     *
     * @attribute data
     * @type CrOnc.NetworkConfigType
     * @default null
     */
    data: null,
  },

  /** @override */
  created: function() {
    this.data = /** @type {CrOnc.NetworkConfigType} */({});
  },

  /** @return {boolean} True if the network is connected. */
  connected: function() {
    return this.data.ConnectionState == CrOnc.ConnectionState.CONNECTED;
  },

  /** @return {boolean} True if the network is connecting. */
  connecting: function() {
    return this.data.ConnectionState == CrOnc.ConnectionState.CONNECTING;
  },

  /** @return {number} The signal strength of the network. */
  getStrength: function() {
    var type = this.data.Type;
    var strength = 0;
    if (type == 'WiFi')
      strength = this.data.WiFi ? this.data.WiFi.SignalStrength : 0;
    else if (type == 'Cellular')
      strength = this.data.Cellular ? this.data.Cellular.SignalStrength : 0;
    else if (type == 'WiMAX')
      strength = this.data.WiMAX ? this.data.WiMAX.SignalStrength : 0;
    return strength;
  },

  /** @return {CrOnc.Security} The ONC security type. */
  getWiFiSecurity: function() {
    return (this.data.WiFi && this.data.WiFi.Security) ?
        this.data.WiFi.Security : CrOnc.Security.NONE;
  },

  /** @return {CrOnc.RoamingState} The ONC roaming state. */
  getCellularRoamingState: function() {
    return (this.data.Cellular && this.data.Cellular.RoamingState) ?
        this.data.Cellular.RoamingState : CrOnc.RoamingState.UNKNOWN;
  },

  /** @return {CrOnc.NetworkTechnology} The ONC network technology. */
  getCellularTechnology: function() {
    return (this.data.Cellular && this.data.Cellular.NetworkTechnology) ?
        this.data.Cellular.NetworkTechnology : CrOnc.NetworkTechnology.UNKNOWN;
  }
});
