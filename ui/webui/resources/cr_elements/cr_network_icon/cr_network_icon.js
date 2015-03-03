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
 *   showBadges: boolean,
 *   showDisconnected: boolean,
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
  /**
   * The icon type to use for the base image of the icon.
   * @type string
   */
  iconType: 'ethernet',

  /**
   * Set to true to show a badge for roaming networks.
   * @type boolean
   */
  roaming: false,

  /**
   * Set to true to show a badge for secure networks.
   * @type boolean
   */
  secure: false,

  /**
   * Set to the name of a technology to show show a badge.
   * @type string
   */
  technology: '',

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
      showBadges: false,
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
    this.iconType = getIconTypeFromNetworkType(this.networkState.data.Type);
    var params = /** @type {IconParams} */ {
      showBadges: true,
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
    this.iconType = getIconTypeFromNetworkType(this.networkType);
    var params = /** @type {IconParams} */ {
      showBadges: false,
      showDisconnected: true,
      strength: 0,
    };
    this.setIcon_(params);
  },

  /**
   * Returns the url for an image based on identifier |id|.
   * @param {string} src The identifier describing the image.
   * @return {string} The url to use for the image 'src' property.
   */
  toImageSrc: function(id) {
    return id ? RESOURCE_IMAGE_BASE + id + RESOURCE_IMAGE_EXT : '';
  },

  /**
   * Returns the url for a badge image based on identifier |id|.
   * @param {string} id The identifier describing the badge.
   * @return {string} The url to use for the badge image 'src' property.
   */
  toBadgeImageSrc: function(id) {
    return id ? this.toImageSrc('badge_' + id) : '';
  },

  /**
   * Sets the icon and badge based on the current state and |strength|.
   * @param {!IconParams} params The set of params describing the icon.
   * @private
   */
  setIcon_: function(params) {
    var icon = this.$.icon;

    var multiLevel = (this.iconType == 'wifi' || this.iconType == 'mobile');

    if (this.networkState && multiLevel) {
      this.setMultiLevelIcon_(params);
    } else {
      icon.className = multiLevel ? 'multi-level level0' : '';
    }

    this.setIconBadges_(params);
  },

  /**
   * Toggles icon classes based on strength and connecting properties.
   * |this.networkState| is expected to be specified.
   * @param {!IconParams} params The set of params describing the icon.
   * @private
   */
  setMultiLevelIcon_: function(params) {
    // Set the strength or connecting properties.
    var networkState = this.networkState;

    var connecting = false;
    var strength = -1;
    if (networkState.connecting()) {
      strength = 0;
      connecting = true;
    } else if (networkState.connected() || !params.showDisconnected) {
      strength = params.strength || 0;
    }

    var icon = this.$.icon;
    icon.classList.toggle('multi-level', true);
    icon.classList.toggle('connecting', connecting);
    icon.classList.toggle('level0', strength < 0);
    icon.classList.toggle('level1', strength >= 0 && strength <= 25);
    icon.classList.toggle('level2', strength > 25 && strength <= 50);
    icon.classList.toggle('level3', strength > 50 && strength <= 75);
    icon.classList.toggle('level4', strength > 75);
  },

  /**
   * Sets the icon badge visibility properties: roaming, secure, technology.
   * @param {!IconParams} params The set of params describing the icon.
   * @private
   */
  setIconBadges_: function(params) {
    var networkState = this.networkState;

    var type =
        (params.showBadges && networkState) ? networkState.data.Type : '';
    if (type == CrOnc.Type.WIFI) {
      this.roaming = false;
      this.secure = networkState.getWiFiSecurity() != 'None';
      this.technology = '';
    } else if (type == CrOnc.Type.WIMAX) {
      this.roaming = false;
      this.secure = false;
      this.technology = '4g';
    } else if (type == CrOnc.Type.CELLULAR) {
      this.roaming =
          networkState.getCellularRoamingState() == CrOnc.RoamingState.ROAMING;
      this.secure = false;
      var oncTechnology = networkState.getCellularTechnology();
      switch (oncTechnology) {
        case CrOnc.NetworkTechnology.EDGE:
          this.technology = 'edge';
          break;
        case CrOnc.NetworkTechnology.EVDO:
          this.technology = 'evdo';
          break;
        case CrOnc.NetworkTechnology.GPRS:
        case CrOnc.NetworkTechnology.GSM:
          this.technology = 'gsm';
          break;
        case CrOnc.NetworkTechnology.HSPA:
          this.technology = 'hspa';
          break;
        case CrOnc.NetworkTechnology.HSPA_PLUS:
          this.technology = 'hspa_plus';
          break;
        case CrOnc.NetworkTechnology.LTE:
          this.technology = 'lte';
          break;
        case CrOnc.NetworkTechnology.LTE_ADVANCED:
          this.technology = 'lte_advanced';
          break;
        case CrOnc.NetworkTechnology.UMTS:
          this.technology = '3g';
          break;
      }
    } else {
      this.roaming = false;
      this.secure = false;
      this.technology = '';
    }
  },
});
})();
