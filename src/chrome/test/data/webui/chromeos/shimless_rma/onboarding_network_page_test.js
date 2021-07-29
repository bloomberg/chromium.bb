// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {fakeNetworks} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setNetworkConfigServiceForTesting, setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingNetworkPage} from 'chrome://shimless-rma/onboarding_network_page.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';
import {FakeNetworkConfig} from '../fake_network_config_mojom.m.js';

export function onboardingNetworkPageTest() {
  /** @type {?OnboardingNetworkPageElement} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let shimlessRmaService = null;

  /** @type {?FakeNetworkConfig} */
  let networkConfigService = null;

  suiteSetup(() => {
    shimlessRmaService = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(shimlessRmaService);
    networkConfigService = new FakeNetworkConfig();
    setNetworkConfigServiceForTesting(networkConfigService);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    component.remove();
    component = null;
    shimlessRmaService.reset();
    networkConfigService.resetForTest();
  });

  /**
   * @return {!Promise}
   */
  function initializeChooseDestinationPage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingNetworkPageElement} */ (
        document.createElement('onboarding-network-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  /**
   * @return {!Promise}
   */
  function openNetworkConfigDialog() {
    assertTrue(!!component);

    const networkList = component.shadowRoot.querySelector('#networkList');
    const network = networkList.networks[1];
    component.showConfig_(
        network.type,
        /* empty guid since network_config.m.js is not mocked */ undefined,
        'eth0');

    return flushTasks();
  }

  test('ComponentRenders', async () => {
    await initializeChooseDestinationPage();
    assertTrue(!!component);

    const networkList = component.shadowRoot.querySelector('#networkList');
    assertTrue(!!networkList);
  });


  test('PopulatesNetworkList', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeChooseDestinationPage();

    const networkList = component.shadowRoot.querySelector('#networkList');
    assertTrue(!!networkList);
    assertEquals(networkList.networks[0].guid, 'eth0_guid');
    assertEquals(networkList.networks[1].guid, 'wifi0_guid');
  });

  test('NetworkSelectionDialog', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeChooseDestinationPage();

    const networkList = component.shadowRoot.querySelector('#networkList');
    component.onNetworkSelected_({detail: networkList.networks[1]});
    await flushTasks();

    const networkDialog = component.shadowRoot.querySelector('#networkConfig');
    assertTrue(!!networkDialog);
    assertFalse(networkDialog.enableConnect);

    const connectButton = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#connectButton'));
    assertTrue(connectButton.disabled);
  });

  test('DialogConnectButtonBindsToDialog', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeChooseDestinationPage();
    await openNetworkConfigDialog();

    const connectButton = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#connectButton'));
    assertTrue(connectButton.disabled);

    const networkDialog = component.shadowRoot.querySelector('#networkConfig');
    networkDialog.enableConnect = true;
    await flushTasks();

    assertFalse(connectButton.disabled);
  });

  test('DialogCloses', async () => {
    networkConfigService.addNetworksForTest(fakeNetworks);
    await initializeChooseDestinationPage();

    const dialog = /** @type {!CrDialogElement} */ (
        component.shadowRoot.querySelector('#dialog'));
    assertFalse(dialog.open);

    await openNetworkConfigDialog();

    const cancelButton = component.shadowRoot.querySelector('#cancelButton');
    assertFalse(cancelButton.disabled);
    assertTrue(dialog.open);

    cancelButton.click();
    await flushTasks();

    assertFalse(dialog.open);
  });
}
