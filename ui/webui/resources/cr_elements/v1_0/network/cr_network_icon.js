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
    'chrome://resources/cr_elements/network/';

/** @const {string} */ var RESOURCE_IMAGE_EXT = '.png';

/**
 * Gets the icon type from the network type. This allows multiple types
 * (i.e. Cellular, WiMAX) to map to the same icon type (i.e. mobile).
 * @param {CrOnc.Type} networkType The ONC network type.
 * @return {string} The icon type: ethernet, wifi, mobile, or vpn.
 */
function getIconTypeFromNetworkType(networkType) {
  if (!networkType || networkType == CrOnc.Type.ETHERNET)
    return 'ethernet';
  else if (networkType == CrOnc.Type.WIFI)
    return 'wifi';
  else if (networkType == CrOnc.Type.CELLULAR)
    return 'mobile';
  else if (networkType == CrOnc.Type.WIMAX)
    return 'mobile';
  else if (networkType == CrOnc.Type.VPN)
    return 'vpn';

  console.error('Unrecognized network type for icon: ' + networkType);
  return 'ethernet';
}

/**
 * Polymer class definition for 'cr-network-icon'.
 * @element cr-network-icon
 */
Polymer({
  is: 'cr-network-icon',

  properties: {
    /**
     * If set, the ONC state properties will be used to display the icon.
     *
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null,
      observer: 'networkStateChanged_'
    },

    /**
     * If set, the ONC network type will be used to display the icon.
     *
     * @type {?CrOnc.Type}
     */
    networkType:  {
      type: String,
      value: undefined,
      observer: 'networkTypeChanged_'
    },

    /**
     * If true, the icon is part of a list of networks and may be displayed
     * differently, e.g. the disconnected image will never be shown for
     * list items.
     */
    isListItem:  {
      type: Boolean,
      value: false,
      observer: 'isListItemChanged_'
    },

    /**
     * The icon type to use for the base image of the icon.
     * @private
     */
    iconType_: {
      type: String,
      value: 'ethernet',
    },

    /**
     * Set to true to show a badge for roaming networks.
     * @private
     */
    roaming_: {
      type: Boolean,
      value: false,
    },

    /**
     * Set to true to show a badge for secure networks.
     * @private
     */
    secure_: {
      type: Boolean,
      value: false,
    },

    /**
     * Set to the name of a technology to show show a badge.
     * @private
     */
    technology_: {
      type: String,
      value: '',
    },
  },

  /**
   * Polymer networkState changed method. Updates the icon based on the
   * network state.
   * @private
   */
  networkStateChanged_: function() {
    if (!this.networkState)
      return;

    this.networkType = undefined;
    this.iconType_ = getIconTypeFromNetworkType(this.networkState.Type);
    var strength = /** @type {number} */ (
        CrOnc.getTypeProperty(this.networkState, 'SignalStrength') || 0);
    var params = /** @type {IconParams} */ {
      showBadges: true,
      showDisconnected: !this.isListItem,
      strength: strength
    };
    this.setIcon_(params);
  },

  /**
   * Polymer networkType changed method. Updates the icon based on the type
   * of network, showing a disconnected icon where appropriate and no badges.
   * @private
   */
  networkTypeChanged_: function() {
    if (!this.networkType)
      return;

    this.networkState = null;
    this.iconType_ = getIconTypeFromNetworkType(this.networkType);
    var params = /** @type {IconParams} */ {
      showBadges: false,
      showDisconnected: true,
      strength: 0,
    };
    this.setIcon_(params);
  },

  /**
   * Polymer isListItem changed method.
   * @private
   */
  isListItemChanged_: function() {
    if (this.networkState)
      this.networkStateChanged_();
    else if (this.networkType)
      this.networkTypeChanged_();
  },

  /**
   * Returns the url for an image based on identifier |id|.
   * @param {string} id The identifier describing the image.
   * @return {string} The url to use for the image 'src' property.
   * @private
   */
  toImageSrc_: function(id) {
    return id ? RESOURCE_IMAGE_BASE + id + RESOURCE_IMAGE_EXT : '';
  },

  /**
   * Returns the url for a badge image based on identifier |id|.
   * @param {string} id The identifier describing the badge.
   * @return {string} The url to use for the badge image 'src' property.
   * @private
   */
  toBadgeImageSrc_: function(id) {
    return id ? this.toImageSrc_('badge_' + id) : '';
  },

  /**
   * Sets the icon and badge based on the current state and |strength|.
   * @param {!IconParams} params The set of params describing the icon.
   * @private
   */
  setIcon_: function(params) {
    var icon = this.$.icon;

    var multiLevel = (this.iconType_ == 'wifi' || this.iconType_ == 'mobile');

    if (this.networkState && multiLevel) {
      this.setMultiLevelIcon_(params);
    } else {
      icon.classList.toggle('multi-level', multiLevel);
      icon.classList.toggle('level0', multiLevel);
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

    var connectionState = networkState.ConnectionState;
    var connecting = false;
    var strength = -1;
    if (connectionState == CrOnc.ConnectionState.CONNECTING) {
      strength = 0;
      connecting = true;
    } else if (connectionState == CrOnc.ConnectionState.CONNECTED ||
               !params.showDisconnected) {
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
        (params.showBadges && networkState) ? networkState.Type : '';
    if (type == CrOnc.Type.WIFI) {
      this.roaming_ = false;
      var security = CrOnc.getTypeProperty(networkState, 'Security');
      this.secure_ = security && security != 'None';
      this.technology_ = '';
    } else if (type == CrOnc.Type.WIMAX) {
      this.roaming_ = false;
      this.secure_ = false;
      this.technology_ = '4g';
    } else if (type == CrOnc.Type.CELLULAR) {
      this.roaming_ = CrOnc.getTypeProperty(networkState, 'RoamingState') ==
                      CrOnc.RoamingState.ROAMING;
      this.secure_ = false;
      var oncTechnology =
          CrOnc.getTypeProperty(networkState, 'NetworkTechnology');
      switch (oncTechnology) {
        case CrOnc.NetworkTechnology.CDMA1XRTT:
          this.technology_ = '1x';
          break;
        case CrOnc.NetworkTechnology.EDGE:
          this.technology_ = 'edge';
          break;
        case CrOnc.NetworkTechnology.EVDO:
          this.technology_ = 'evdo';
          break;
        case CrOnc.NetworkTechnology.GPRS:
        case CrOnc.NetworkTechnology.GSM:
          this.technology_ = 'gsm';
          break;
        case CrOnc.NetworkTechnology.HSPA:
          this.technology_ = 'hspa';
          break;
        case CrOnc.NetworkTechnology.HSPA_PLUS:
          this.technology_ = 'hspa_plus';
          break;
        case CrOnc.NetworkTechnology.LTE:
          this.technology_ = 'lte';
          break;
        case CrOnc.NetworkTechnology.LTE_ADVANCED:
          this.technology_ = 'lte_advanced';
          break;
        case CrOnc.NetworkTechnology.UMTS:
          this.technology_ = '3g';
          break;
      }
    } else {
      this.roaming_ = false;
      this.secure_ = false;
      this.technology_ = '';
    }
  },
});
})();
