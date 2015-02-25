// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for rendering network icons based on ONC
 * state properties.
 */

(function() {
/**
 * @typedef {{
 *   showDisconnected: boolean,
 *   iconType: string,
 *   connected: boolean,
 *   secure: boolean,
 *   strength: number
 * }}
 */
var IconParams;

/** @const {string} */ var RESOURCE_IMAGE_BASE =
    'chrome://resources/cr_elements/cr_network_icon/';

/** @const {string} */ var RESOURCE_IMAGE_EXT = '.png';

/**
 * Gets the icon type from the network type. This allows multiple types
 * (i.e. Cellular, WiMAX) to map to the same icon type (i.e. mobile).
 * @param {CrOnc.Type} networkType The ONC network type.
 * @return {string} The icon type: ethernet, wifi, mobile, or vpn.
 */
function getIconTypeFromNetworkType(networkType) {
  if (networkType == CrOnc.Type.ETHERNET)
    return 'ethernet';
  else if (networkType == CrOnc.Type.WIFI)
    return 'wifi';
  else if (networkType == CrOnc.Type.CELLULAR)
    return 'mobile';
  else if (networkType == CrOnc.Type.WIMAX)
    return 'mobile';
  else if (networkType == CrOnc.Type.VPN)
    return 'vpn';

  console.error('Unrecognized network type: ' + networkType);
  return 'ethernet';
}

/**
 * Polymer class definition for 'cr-network-icon'.
 * @element cr-network-icon
 */
Polymer('cr-network-icon', {
  publish: {
    /**
     * If set, the ONC data properties will be used to display the icon.
     *
     * @attribute networkState
     * @type CrOncDataElement
     * @default null
     */
    networkState: null,

    /**
     * If set, the ONC network type will be used to display the icon.
     *
     * @attribute networkType
     * @type CrOnc.Type
     * @default null
     */
    networkType: null,

    /**
     * If true, the icon is part of a list of networks and may be displayed
     * differently, e.g. the disconnected image will never be shown for
     * list items.
     *
     * @attribute isListItem
     * @type boolean
     * @default false
     */
    isListItem: false,
  },

  /** @override */
  attached: function() {
    var params = /** @type {IconParams} */ {
      connected: false,
      iconType: 'ethernet',
      secure: false,
      showDisconnected: true,
      strength: 0,
    };
    this.setIcon_(params);
  },

  /**
   * Polymer networkState changed method. Updates the icon based on the
   * network state.
   */
  networkStateChanged: function() {
    var iconType = getIconTypeFromNetworkType(this.networkState.data.Type);
    var params = /** @type {IconParams} */ {
      connected: this.networkState.data.ConnectionState == 'Connected',
      iconType: iconType,
      secure: iconType == 'wifi' &&
          this.networkState.getWiFiSecurity() != 'None',
      showDisconnected: !this.isListItem,
      strength: this.networkState.getStrength(),
    };
    this.setIcon_(params);
  },

  /**
   * Polymer networkType changed method. Updates the icon based on the type
   * of network, showing a disconnected icon where appropriate and no badges.
   */
  networkTypeChanged: function() {
    var params = /** @type {IconParams} */ {
      connected: false,
      iconType: getIconTypeFromNetworkType(this.networkType),
      secure: false,
      showDisconnected: true,
      strength: 0,
    };
    this.setIcon_(params);
  },

  /**
   * Sets the icon and badge based on the current state and |strength|.
   * @param {!IconParams} params The set of params describing the icon.
   * @private
   */
  setIcon_: function(params) {
    var icon = this.$.icon;
    if (params.iconType)
      icon.src = RESOURCE_IMAGE_BASE + params.iconType + RESOURCE_IMAGE_EXT;

    var multiLevel, strength;
    if (params.iconType == 'wifi' || params.iconType == 'mobile') {
      multiLevel = true;
      strength = (params.showDisconnected && !params.connected) ?
          -1 : params.strength;
    } else {
      multiLevel = false;
      strength = -1;
    }
    icon.classList.toggle('multi-level', multiLevel);
    icon.classList.toggle('level0', strength < 0);
    icon.classList.toggle('level1', strength >= 0 && strength <= 25);
    icon.classList.toggle('level2', strength > 25 && strength <= 50);
    icon.classList.toggle('level3', strength > 50 && strength <= 75);
    icon.classList.toggle('level4', strength > 75);

    this.$.secure.hidden = !params.secure;
  },
});
})();
