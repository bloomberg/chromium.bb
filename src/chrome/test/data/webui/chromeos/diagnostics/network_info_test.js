// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_info.js';

import {Network} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCellularNetwork, fakeEthernetNetwork, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function networkInfoTestSuite() {
  /** @type {?NetworkInfoElement} */
  let networkInfoElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    networkInfoElement.remove();
    networkInfoElement = null;
  });

  /**
   * @param {!Network} network
   */
  function initializeNetworkInfo(network) {
    assertFalse(!!networkInfoElement);

    // Add the network info to the DOM.
    networkInfoElement = /** @type {!NetworkInfoElement} */ (
        document.createElement('network-info'));
    assertTrue(!!networkInfoElement);
    networkInfoElement.network = network;
    document.body.appendChild(networkInfoElement);

    return flushTasks();
  }

  /**
   * @param {!Network} network
   * @return {!Promise}
   */
  function changeNetwork(network) {
    networkInfoElement.network = network;
    return flushTasks();
  }

  test('CorrectInfoElementShown', () => {
    return initializeNetworkInfo(fakeWifiNetwork)
        .then(() => {
          // wifi-info should be visible.
          assertTrue(
              isVisible(dx_utils.getWifiInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getEthernetInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getCellularInfoElement(networkInfoElement)));
          return changeNetwork(fakeCellularNetwork);
        })
        .then(() => {
          // cellular-info should be visible.
          assertTrue(
              isVisible(dx_utils.getCellularInfoElement(networkInfoElement)));

          assertFalse(
              isVisible(dx_utils.getWifiInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getEthernetInfoElement(networkInfoElement)));
          return changeNetwork(fakeEthernetNetwork);
        })
        .then(() => {
          // ethernet-info should be visible.
          assertTrue(
              isVisible(dx_utils.getEthernetInfoElement(networkInfoElement)));

          assertFalse(
              isVisible(dx_utils.getWifiInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getCellularInfoElement(networkInfoElement)));
        });
  });
}