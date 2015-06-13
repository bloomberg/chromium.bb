// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the IP Config properties fo
 * a network state. TODO(stevenjb): Allow editing of static IP configurations
 * when 'editiable' is true.
 */
Polymer({
  is: 'cr-network-ip-config',

  properties: {
    /**
     * The current state containing the IP Config properties to display and
     * modify.
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null,
      observer: 'networkStateChanged_'
    },

    /**
     * Whether or not the IP Address can be edited.
     * TODO(stevenjb): Implement editing.
     */
    editable: {
      type: Boolean,
      value: false
    },

    /**
     * State of 'Configure IP Addresses Automatically'.
     */
    automatic: {
      type: Boolean,
      value: false,
      observer: 'automaticChanged_'
    },

    /**
     * IP Config property dictionary.
     * @type {{
     *   ipv4: !CrOnc.IPConfigProperties,
     *   ipv6: !CrOnc.IPConfigProperties
     * }}
     */
    ipConfig: {
      type: Object,
      value: function() { return {ipv4: {}, ipv6: {}}; },
      observer: 'ipConfigChanged_'
    },

    /**
     * Array of properties to pass to the property list.
     */
    ipConfigFields_: {
      type: Array,
      value: function() {
        return [
          'ipv4.IPAddress',
          'ipv4.RoutingPrefix',
          'ipv4.Gateway',
          'ipv6.IPAddress'
        ];
      },
      readOnly: true
    },
  },

  /**
   * Polymer networkState changed method.
   */
  networkStateChanged_: function() {
    if (!this.networkState)
      return;

    // Update the 'automatic' property.
    var ipConfigType =
        CrOnc.getActiveValue(this.networkState, 'IPAddressConfigType');
    this.automatic = (ipConfigType != CrOnc.IPConfigType.STATIC);

    // Update the 'ipConfig' property.
    var ipv4 = {}, ipv6 = {};
    var ipConfigs = CrOnc.getActiveValue(this.networkState, 'IPConfigs');
    if (ipConfigs) {
      for (var i = 0; i < ipConfigs.length; ++i) {
        var ipConfig = ipConfigs[i];
        if (ipConfig.hasOwnProperty('RoutingPrefix')) {
          ipConfig.RoutingPrefix =
              CrOnc.getRoutingPrefixAsAddress(ipConfig.RoutingPrefix);
        }
        if (ipConfig.Type == CrOnc.IPType.IPV4 && !ipv4.IPAddress)
          ipv4 = ipConfig;
        else if (ipConfig.Type == CrOnc.IPType.IPV6 && !ipv6.IPAddress)
          ipv6 = ipConfig;
      }
    }
    this.ipConfig = {ipv4: ipv4, ipv6: ipv6};
  },

  /**
   * Polymer automatic changed method.
   * TODO(stevenjb): Implement editing.
   */
  automaticChanged_: function() {
  },

  /**
   * Polymer ipConfig changed method.
   * TODO(stevenjb): Implement editing.
   */
  ipConfigChanged_: function() {
  },
});
