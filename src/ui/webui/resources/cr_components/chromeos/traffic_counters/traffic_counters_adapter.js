// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class that provides the functionality for interacting with traffic counters.
 */

import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {CrosNetworkConfig} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

/** @type {number} Default traffic counter reset day. */
const kDefaultResetDay = 1;

/**
 * Information about a network.
 * @typedef {{
 *   guid: string,
 *   name: string,
 *   type: !chromeos.networkConfig.mojom.NetworkType,
 *   counters: !Array<!Object>,
 *   lastResetTime: ?mojoBase.mojom.Time,
 *   friendlyDate: ?string,
 *   autoReset: boolean,
 *   userSpecifiedResetDay: number,
 * }}
 */
export let Network;


/**
 * Helper function to create a Network object.
 * @param {string} guid
 * @param {string} name
 * @param {!chromeos.networkConfig.mojom.NetworkType} type
 * @param {!Array<!Object>} counters
 * @param {?mojoBase.mojom.Time} lastResetTime
 * @param {?string} friendlyDate
 * @param {boolean} autoReset
 * @param {number} userSpecifiedResetDay
 * @return {Network} Network object
 */
function createNetwork(
    guid, name, type, counters, lastResetTime, friendlyDate, autoReset,
    userSpecifiedResetDay) {
  return {
    guid: guid,
    name: name,
    type: type,
    counters: counters,
    lastResetTime: lastResetTime,
    friendlyDate: friendlyDate,
    autoReset: autoReset,
    userSpecifiedResetDay: userSpecifiedResetDay,
  };
}

export class TrafficCountersAdapter {
  constructor() {
    /**
     * Network Config mojo remote.
     * @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote}
     */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /**
   * Requests traffic counters for active networks.
   * @return {!Promise<!Array<!Network>>}
   */
  async requestTrafficCountersForActiveNetworks() {
    const filter = {
      filter: chromeos.networkConfig.mojom.FilterType.kActive,
      networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
      limit: chromeos.networkConfig.mojom.NO_LIMIT,
    };
    const networks = [];
    const networkStateList =
        await this.networkConfig_.getNetworkStateList(filter);
    for (const networkState of networkStateList.result) {
      const trafficCounters =
          await this.requestTrafficCountersForNetwork(networkState.guid);
      const lastResetTime =
          await this.requestLastResetTimeForNetwork(networkState.guid);
      const friendlyDate =
          await this.requestFriendlyDateForNetwork(networkState.guid);
      const autoReset =
          await this.requestEnableAutoResetBooleanForNetwork(networkState.guid);
      const userSpecifiedResetDay =
          await this.requestUserSpecifiedResetDayForNetwork(networkState.guid);
      networks.push(createNetwork(
          networkState.guid, networkState.name, networkState.type,
          trafficCounters, lastResetTime, friendlyDate, autoReset,
          userSpecifiedResetDay));
    }
    return networks;
  }

  /**
   * Resets traffic counters for the given network.
   * @param {string} guid
   */
  async resetTrafficCountersForNetwork(guid) {
    await this.networkConfig_.resetTrafficCounters(guid);
  }

  /**
   * Requests traffic counters for the given network.
   * @param {string} guid
   * @return {!Promise<!Array<!Object>>} traffic counters for network with guid
   */
  async requestTrafficCountersForNetwork(guid) {
    const trafficCountersObj =
        await this.networkConfig_.requestTrafficCounters(guid);
    return trafficCountersObj.trafficCounters;
  }

  /**
   * Requests last reset time for the given network.
   * @param {string} guid
   * @return {?Promise<?mojoBase.mojom.Time>} last reset
   * time for network with guid
   */
  async requestLastResetTimeForNetwork(guid) {
    const managedPropertiesPromise =
        await this.networkConfig_.getManagedProperties(guid);
    if (!managedPropertiesPromise || !managedPropertiesPromise.result) {
      return null;
    }
    return managedPropertiesPromise.result.trafficCounterProperties
               .lastResetTime ||
        null;
  }

  /**
   * Requests a reader friendly date, corresponding to the last reset time,
   * for the given network.
   * @param {string} guid
   * @return {?Promise<?string>} friendly date corresponding to the last rest
   * time for network matching |guid|.
   */
  async requestFriendlyDateForNetwork(guid) {
    const managedPropertiesPromise =
        await this.networkConfig_.getManagedProperties(guid);
    if (!managedPropertiesPromise || !managedPropertiesPromise.result) {
      return null;
    }
    return managedPropertiesPromise.result.trafficCounterProperties
               .friendlyDate ||
        null;
  }

  /**
   * Requests enable traffic counters auto reset boolean for the given network.
   * @param {string} guid
   * @return {!Promise<boolean>} whether traffic counter auto reset is enabled
   */
  async requestEnableAutoResetBooleanForNetwork(guid) {
    const managedPropertiesPromise =
        await this.networkConfig_.getManagedProperties(guid);
    if (!managedPropertiesPromise || !managedPropertiesPromise.result) {
      return false;
    }
    return managedPropertiesPromise.result.trafficCounterProperties.autoReset;
  }

  /**
   * Requests user specified reset day for the given network.
   * @param {string} guid
   * @return {!Promise<number>}
   */
  async requestUserSpecifiedResetDayForNetwork(guid) {
    const managedPropertiesPromise =
        await this.networkConfig_.getManagedProperties(guid);
    if (!managedPropertiesPromise || !managedPropertiesPromise.result) {
      return kDefaultResetDay;
    }
    return managedPropertiesPromise.result.trafficCounterProperties
        .userSpecifiedResetDay;
  }

  /**
   * Sets values for auto reset.
   * @param {string} guid
   * @param {boolean} autoReset
   * @param {?chromeos.networkConfig.mojom.UInt32Value} resetDay
   */
  async setTrafficCountersAutoResetForNetwork(guid, autoReset, resetDay) {
    await this.networkConfig_.setTrafficCountersAutoReset(
        guid, autoReset, resetDay);
  }
}
