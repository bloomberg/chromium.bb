// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_list.js';

import {NetworkGuidInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakePowerRoutineResults, fakeRoutineResults, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function networkListTestSuite() {
  /** @type {?NetworkListElement} */
  let networkListElement = null;

  /** @type {?FakeNetworkHealthProvider} */
  let provider = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  suiteSetup(() => {
    provider = new FakeNetworkHealthProvider();
    setNetworkHealthProviderForTesting(provider);

    // Setup a fake routine controller.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        [...fakeRoutineResults.keys(), ...fakePowerRoutineResults.keys()]);

    setSystemRoutineControllerForTesting(routineController);
  });

  teardown(() => {
    networkListElement.remove();
    networkListElement = null;
    provider.reset();
  });

  /**
   * @param {!Array<!NetworkGuidInfo>} fakeNetworkGuidInfoList
   */
  function initializeNetworkList(fakeNetworkGuidInfoList) {
    assertFalse(!!networkListElement);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);
    provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);

    // Add the network list to the DOM.
    networkListElement = /** @type {!NetworkListElement} */ (
        document.createElement('network-list'));
    assertTrue(!!networkListElement);
    document.body.appendChild(networkListElement);

    return flushTasks();
  }

  /**
   * Returns the connectivity-card element.
   * @return {!ConnectivityCardElement}
   */
  function getConnectivityCard() {
    const connectivityCard =
        /** @type {!ConnectivityCardElement} */ (
            networkListElement.$$('connectivity-card'));
    assertTrue(!!connectivityCard);
    return connectivityCard;
  }

  /**
   * Causes the network list observer to fire.
   */
  function triggerNetworkListObserver() {
    provider.triggerNetworkListObserver();
    return flushTasks();
  }

  /**
   * Returns all network-card elements.
   * @return {!NodeList<!Element>}
   */
  function getNetworkCardElements() {
    return networkListElement.shadowRoot.querySelectorAll('network-card');
  }

  /**
   * @param {string} guid
   * @suppress {visibility} // access private member
   * @return {!Promise}
   */
  function changeActiveGuid(guid) {
    networkListElement.activeGuid_ = guid;
    return flushTasks();
  }

  /**
   * Returns list of network guids.
   * @suppress {visibility} // access private member for test
   * @return {Array<?string>}
   */
  function getOtherNetworkGuids() {
    return networkListElement.otherNetworkGuids_;
  }

  test('ActiveGuidPresent', () => {
    // The network-list element sets up a NetworkListObserver as part
    // of its initialization. Registering this observer causes it to
    // fire once.
    return initializeNetworkList(fakeNetworkGuidInfoList).then(() => {
      assertEquals(
          getConnectivityCard().activeGuid,
          fakeNetworkGuidInfoList[0].activeGuid);
    });
  });

  test('ActiveGuidUpdates', () => {
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => triggerNetworkListObserver())
        .then(() => {
          // Triggering the NetworkListObserver provides
          // the second observation: fakeNetworkGuidInfoList[1].
          assertEquals(
              getConnectivityCard().activeGuid,
              fakeNetworkGuidInfoList[1].activeGuid);
        });
  });

  test('NetworkGuidsPresent', () => {
    let networkGuids;
    let numDomRepeatInstances;
    let networkCardElements;
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => {
          networkGuids = getOtherNetworkGuids();
          numDomRepeatInstances =
              networkListElement.$$('#networkCardList').items.length;
          networkCardElements = getNetworkCardElements();
          assertEquals(numDomRepeatInstances, networkGuids.length);
          for (let i = 0; i < networkCardElements.length; i++) {
            assertEquals(networkCardElements[i].guid, networkGuids[i]);
          }
          return triggerNetworkListObserver();
        })
        .then(() => {
          networkGuids = getOtherNetworkGuids();
          numDomRepeatInstances =
              networkListElement.$$('#networkCardList').items.length;
          networkCardElements = getNetworkCardElements();
          assertEquals(numDomRepeatInstances, networkGuids.length);
          for (let i = 0; i < networkCardElements.length; i++) {
            assertEquals(networkCardElements[i].guid, networkGuids[i]);
          }
        });
  });

  test('NetworkCardElementsPopulated', () => {
    let networkCardElements;
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => flushTasks())
        .then(() => {
          networkCardElements = getNetworkCardElements();

          // The first network list observation provides guids for Cellular
          // and WiFi. The connectivity-card is responsbile for the Ethernet
          // guid as it's the currently active guid.
          const wifiInfoElement = dx_utils.getWifiInfoElement(
              networkCardElements[0].$$('network-info'));
          const cellularInfoElement = dx_utils.getCellularInfoElement(
              networkCardElements[1].$$('network-info'));
          dx_utils.assertTextContains(
              wifiInfoElement.$$('#ssid').value,
              fakeWifiNetwork.typeProperties.wifi.ssid);
          dx_utils.assertTextContains(
              cellularInfoElement.$$('#name').value, fakeCellularNetwork.name);

          assertEquals(
              getConnectivityCard().activeGuid,
              fakeNetworkGuidInfoList[0].activeGuid);

          return triggerNetworkListObserver();
        })
        .then(() => flushTasks())
        .then(() => {
          networkCardElements = getNetworkCardElements();
          const cellularInfoElement = dx_utils.getCellularInfoElement(
              networkCardElements[0].$$('network-info'));
          dx_utils.assertTextContains(
              cellularInfoElement.$$('#name').value, fakeCellularNetwork.name);
          assertEquals(
              getConnectivityCard().activeGuid,
              fakeNetworkGuidInfoList[1].activeGuid);
        });
  });

  test('ConnectivityCardHiddenWithNoActiveGuid', () => {
    return initializeNetworkList(fakeNetworkGuidInfoList)
        .then(() => changeActiveGuid(''))
        .then(() => assertFalse(isVisible(getConnectivityCard())));
  });
}
