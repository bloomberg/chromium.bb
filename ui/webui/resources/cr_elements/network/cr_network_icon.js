// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for rendering network icons based on ONC
 * state properties.
 */

Polymer({
  is: 'cr-network-icon',

  properties: {
    /**
     * If set, the ONC properties will be used to display the icon. This may
     * either be the complete set of NetworkProperties or the subset of
     * NetworkStateProperties.
     * @type {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
     */
    networkState: Object,

    /**
     * If true, the icon is part of a list of networks and may be displayed
     * differently, e.g. the disconnected image will never be shown for
     * list items.
     */
    isListItem: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @return {string} The name of the svg icon image to show.
   * @private
   */
  getIcon_: function() {
    if (!this.networkState)
      return '';
    let showDisconnected =
        !this.isListItem && (!this.networkState.ConnectionState ||
                             this.networkState.ConnectionState ==
                                 CrOnc.ConnectionState.NOT_CONNECTED);

    switch (this.networkState.Type) {
      case CrOnc.Type.ETHERNET: {
        return 'network:settings-ethernet';
      }
      case CrOnc.Type.VPN: {
        return 'network:vpn-key';
      }
      case CrOnc.Type.CELLULAR: {
        let strength =
            showDisconnected ? 0 : CrOnc.getSignalStrength(this.networkState);
        let index = this.strengthToIndex_(strength);
        return 'network:signal-cellular-' + index.toString(10) + '-bar';
      }
      case CrOnc.Type.WI_FI:
      case CrOnc.Type.WI_MAX: {
        if (showDisconnected)
          return 'network:signal-wifi-off';
        let strength = CrOnc.getSignalStrength(this.networkState);
        let index = this.strengthToIndex_(strength);
        return 'network:signal-wifi-' + index.toString(10) + '-bar';
      }
      default:
        assertNotReached();
    }
    return '';
  },

  /**
   * @param {number} strength The signal strength from [0 - 100].
   * @return {number} An index from 0-4 corresponding to |strength|.
   * @private
   */
  strengthToIndex_: function(strength) {
    if (strength == 0)
      return 0;
    return Math.min(Math.trunc((strength - 1) / 25) + 1, 4);
  },

  /**
   * @return {boolean}
   * @private
   */
  showTechnology_: function() {
    return this.getTechnology_() != '';
  },

  /**
   * @return {string}
   * @private
   */
  getTechnology_: function() {
    let networkState = this.networkState;
    if (!networkState)
      return '';
    let type = networkState.Type;
    if (type == CrOnc.Type.WI_MAX)
      return 'network:4g';
    if (type == CrOnc.Type.CELLULAR && networkState.Cellular) {
      let technology =
          this.getTechnologyId_(networkState.Cellular.NetworkTechnology);
      if (technology != '')
        return 'network:' + technology;
    }
    return '';
  },

  /**
   * @param {string|undefined} networkTechnology
   * @return {string}
   * @private
   */
  getTechnologyId_: function(networkTechnology) {
    switch (networkTechnology) {
      case CrOnc.NetworkTechnology.CDMA1XRTT:
        return 'badge-1x';
      case CrOnc.NetworkTechnology.EDGE:
        return 'badge-edge';
      case CrOnc.NetworkTechnology.EVDO:
        return 'badge-evdo';
      case CrOnc.NetworkTechnology.GPRS:
      case CrOnc.NetworkTechnology.GSM:
        return 'badge-gsm';
      case CrOnc.NetworkTechnology.HSPA:
        return 'badge-hspa';
      case CrOnc.NetworkTechnology.HSPA_PLUS:
        return 'badge-hspa-plus';
      case CrOnc.NetworkTechnology.LTE:
        return 'badge-lte';
      case CrOnc.NetworkTechnology.LTE_ADVANCED:
        return 'badge-lte-advanced';
      case CrOnc.NetworkTechnology.UMTS:
        return 'badge-3g';
    }
    return '';
  },

  /**
   * @return {boolean}
   * @private
   */
  showSecure_: function() {
    let networkState = this.networkState;
    if (!this.networkState)
      return false;
    if (networkState.Type == CrOnc.Type.WI_FI && networkState.WiFi) {
      let security = networkState.WiFi.Security;
      return !!security && security != 'None';
    }
    return false;
  },
});
