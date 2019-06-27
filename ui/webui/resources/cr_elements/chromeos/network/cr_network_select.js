// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping cr-network-list including the
 * networkConfig mojo API calls to populate it.
 */

(function() {

const mojom = chromeos.networkConfig.mojom;

Polymer({
  is: 'cr-network-select',

  behaviors: [CrNetworkListenerBehavior],

  properties: {
    /**
     * Show all buttons in list items.
     */
    showButtons: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * The list of custom items to display after the list of networks.
     * See CrNetworkList for details.
     * @type {!Array<CrNetworkList.CustomItemState>}
     */
    customItems: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** Whether to show technology badges on mobile network icons. */
    showTechnologyBadge: {type: Boolean, value: true},

    /**
     * Whether to show a progress indicator at the top of the network list while
     * a scan (e.g., for nearby Wi-Fi networks) is in progress.
     */
    showScanProgress: {type: Boolean, value: false},

    /**
     * List of all network state data for all visible networks.
     * @private {!Array<!OncMojo.NetworkStateProperties>}
     */
    networkStateList_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Whether a network scan is currently in progress.
     * @private
     */
    isScanOngoing_: {type: Boolean, value: false},

    /**
     * Cached Cellular Device state or undefined if there is no Cellular device.
     * @private {!OncMojo.DeviceStateProperties|undefined} deviceState
     */
    cellularDeviceState_: Object,
  },

  /** @type {!OncMojo.NetworkStateProperties|undefined} */
  defaultNetworkState_: undefined,


  /** @private {number|null} */
  scanIntervalId_: null,

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigProxy} */
  networkConfigProxy_: null,

  /** @override */
  created: function() {
    this.networkConfigProxy_ =
        network_config.MojoInterfaceProviderImpl.getInstance()
            .getMojoServiceProxy();
  },

  /** @override */
  attached: function() {
    this.refreshNetworks();

    const INTERVAL_MS = 10 * 1000;
    const kAll = chromeos.networkConfig.mojom.NetworkType.kAll;
    this.networkConfigProxy_.requestNetworkScan(kAll);
    this.scanIntervalId_ = window.setInterval(function() {
      this.networkConfigProxy_.requestNetworkScan(kAll);
    }.bind(this), INTERVAL_MS);
  },

  /** @override */
  detached: function() {
    if (this.scanIntervalId_ !== null) {
      window.clearInterval(this.scanIntervalId_);
    }
  },

  /**
   * Returns network list object for testing.
   */
  getNetworkListForTest: function() {
    return this.$.networkList.$$('#networkList');
  },

  focus: function() {
    this.$.networkList.focus();
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged: function() {
    this.refreshNetworks();
  },

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged: function() {
    this.refreshNetworks();
  },

  /**
   * Requests the device and network states. May be called externally to force a
   * refresh and list update (e.g. when the element is shown).
   */
  refreshNetworks: function() {
    this.networkConfigProxy_.getDeviceStateList().then(response => {
      this.onGetDeviceStates_(response.result);
    });
  },

  /**
   * Returns default network if it is present.
   * @return {!OncMojo.NetworkStateProperties|undefined}
   */
  getDefaultNetwork: function() {
    let defaultNetwork;
    for (let i = 0; i < this.networkStateList_.length; ++i) {
      const state = this.networkStateList_[i];
      if (OncMojo.connectionStateIsConnected(state.connectionState)) {
        defaultNetwork = state;
        break;
      }
      if (state.connectionState == mojom.ConnectionStateType.kConnecting &&
          !defaultNetwork) {
        defaultNetwork = state;
        // Do not break here in case a non WiFi network is connecting but a
        // WiFi network is connected.
      } else if (state.type == mojom.NetworkType.kWiFi) {
        break;  // Non connecting or connected WiFI networks are always last.
      }
    }
    return defaultNetwork;
  },

  /**
   * Returns network with specified GUID if it is available.
   * @param {string} guid of network.
   * @return {!OncMojo.NetworkStateProperties|undefined}
   */
  getNetwork: function(guid) {
    return this.networkStateList_.find(function(network) {
      return network.guid == guid;
    });
  },

  /**
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStates
   * @private
   */
  onGetDeviceStates_: function(deviceStates) {
    this.isScanOngoing_ =
        deviceStates.some((deviceState) => !!deviceState.scanning);

    const filter = {
      filter: chromeos.networkConfig.mojom.FilterType.kVisible,
      networkType: mojom.NetworkType.kAll,
      limit: chromeos.networkConfig.mojom.kNoLimit,
    };
    this.networkConfigProxy_.getNetworkStateList(filter).then(response => {
      this.onGetNetworkStateList_(deviceStates, response.result);
    });
  },

  /**
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStates
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStates
   * @private
   */
  onGetNetworkStateList_: function(deviceStates, networkStates) {
    this.cellularDeviceState_ = deviceStates.find(function(device) {
      return device.type == mojom.NetworkType.kCellular;
    });
    if (this.cellularDeviceState_) {
      this.ensureCellularNetwork_(networkStates);
    }
    this.networkStateList_ = networkStates;
    this.fire('network-list-changed', networkStates);

    const defaultNetwork = this.getDefaultNetwork();

    if ((!defaultNetwork && !this.defaultNetworkState_) ||
        (defaultNetwork && this.defaultNetworkState_ &&
         defaultNetwork.guid == this.defaultNetworkState_.guid &&
         defaultNetwork.connectionState ==
             this.defaultNetworkState_.connectionState)) {
      return;  // No change to network or ConnectionState
    }
    this.defaultNetworkState_ = defaultNetwork ?
        /** @type {!OncMojo.NetworkStateProperties|undefined} */ (
            Object.assign({}, defaultNetwork)) :
        undefined;
    // Note: event.detail will be {} if defaultNetwork is undefined.
    this.fire('default-network-changed', defaultNetwork);
  },

  /**
   * Modifies |networkStates| to include a cellular network if one is required
   * but does not exist.
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStates
   * @private
   */
  ensureCellularNetwork_: function(networkStates) {
    if (networkStates.find(function(network) {
          return network.type == mojom.NetworkType.kCellular;
        })) {
      return;
    }
    const deviceState = this.cellularDeviceState_.deviceState;
    if (deviceState == mojom.DeviceStateType.kDisabled ||
        deviceState == mojom.DeviceStateType.kProhibited) {
      return;  // No Cellular network
    }

    const cellular =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kCellular);
    cellular.cellular.scanning = this.cellularDeviceState_.scanning;

    // Note: the default connectionState is kNotConnected.
    // TODO(khorimoto): Maybe set an 'initializing' CellularState property if
    // the device state is initializing, see TODO in cr_network_list_item.js.

    // Insert the Cellular network after the Ethernet network if it exists.
    const idx = (networkStates.length > 0 &&
                 networkStates[0].type == mojom.NetworkType.kEthernet) ?
        1 :
        0;
    networkStates.splice(idx, 0, cellular);
  },

  /**
   * Event triggered when a cr-network-list-item is selected.
   * @param {!{target: HTMLElement, detail: !OncMojo.NetworkStateProperties}} e
   * @private
   */
  onNetworkListItemSelected_: function(e) {
    const state = e.detail;
    e.target.blur();

    this.fire('network-item-selected', state);
  },
});
})();
