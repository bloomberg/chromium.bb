// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying always-on VPN
 * settings.
 */
(function() {

const mojom = chromeos.networkConfig.mojom;

Polymer({
  is: 'network-always-on-vpn',

  behaviors: [I18nBehavior],

  properties: {

    /**
     * List of all always-on VPN compatible network states.
     * @type {!Array<!OncMojo.NetworkStateProperties>}
     */
    networks: Array,

    /**
     * Always-on VPN operating mode.
     * @type {!chromeos.networkConfig.mojom.AlwaysOnVpnMode|undefined}
     */
    mode: {
      type: Number,
      notify: true,
    },

    /**
     * Always-on VPN service automatically started on login.
     * @type {string|undefined}
     */
    service: {
      type: String,
      notify: true,
    },
  },

  /**
   * Tells whether the always-on VPN main toggle is disabled or not. The toggle
   * is disabled when there's no compatible VPN networks available.
   * @return {boolean}
   * @private
   */
  shouldDisableAlwaysOnVpn_() {
    return this.networks.length === 0;
  },

  /**
   * Computes the visibility of always-on VPN networks list and lockdown toggle.
   * These settings are visible when always-on VPN is enabled.
   * @return {boolean}
   * @private
   */
  shouldShowAlwaysOnVpnOptions_() {
    return !this.shouldDisableAlwaysOnVpn_() &&
        this.mode !== mojom.AlwaysOnVpnMode.kOff;
  },

  /**
   * Computes the checked value for the always-on VPN enabled toggle.
   * @returns {boolean}
   * @private
   */
  computeAlwaysOnVpnEnabled_() {
    return !this.shouldDisableAlwaysOnVpn_() &&
        this.mode !== mojom.AlwaysOnVpnMode.kOff;
  },

  /**
   * Handles a state change on always-on VPN enable toggle.
   * @param {!Event} event
   * @private
   */
  onAlwaysOnEnableChanged_(event) {
    if (!event.target.checked) {
      this.mode = mojom.AlwaysOnVpnMode.kOff;
      return;
    }
    this.mode = mojom.AlwaysOnVpnMode.kBestEffort;
  },

  /**
   * Deduces the lockdown state from the always-on VPN mode.
   * @return {boolean}
   * @private
   */
  computeAlwaysOnVpnLockdown_() {
    return this.mode === mojom.AlwaysOnVpnMode.kStrict;
  },

  /**
   * Handles a lockdown toggle state change. It reflects the change on the
   * current always-on VPN mode.
   * @param {!Event} event
   * @private
   */
  onAlwaysOnVpnLockdownChanged_(event) {
    if (this.mode === mojom.AlwaysOnVpnMode.kOff) {
      // The event should not be fired when always-on VPN is disabled (the
      // enable toggle is disabled).
      return;
    }
    this.mode = event.target.checked ?
        mojom.AlwaysOnVpnMode.kStrict :
        mojom.AlwaysOnVpnMode.kBestEffort;
  },

  /**
   * Returns a list of options for a settings-dropdown-menu from a network
   * state list.
   * @return {!Array<{name: string, value: string, selected: boolean}>}
   * @private
   */
  getAlwaysOnVpnListOptions_() {
    /** @type {!Array<{name: string, value: string, selected: boolean}>} */
    const options = [];
    /** @type {string} */
    const currentService = /** @type {string} */ (this.service);
    /** @type {boolean} */
    let serviceIsInList = false;

    if (!this.networks) {
      return options;
    }

    // Build a list of options for the service dropdown menu.
    this.networks.forEach(state => {
      options.push({
        name: state.name,
        value: state.guid,
        selected: currentService === state.guid,
      });
      serviceIsInList |= (currentService === state.guid);
    });

    // The current always-on VPN service is not in the VPN network list, it
    // needs a placeholder.
    if (!serviceIsInList) {
      options.unshift({
        name: '',
        value: '',
        selected: true,
      });
    }

    return options;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAlwaysOnVpnServiceChanged_(event) {
    this.service = /** @type {string} */ (event.target.value);
  },

});
})();