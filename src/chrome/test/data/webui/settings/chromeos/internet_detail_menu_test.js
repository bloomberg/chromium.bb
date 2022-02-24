// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {routes, Router} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertTrue} from '../../chai_assert.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {eventToPromise, flushTasks, waitAfterNextRender} from 'chrome://test/test_util.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// clang-format on

suite('InternetDetailMenu', function() {
  let internetDetailMenu;
  let mojoApi_;
  let mojom;
  let eSimManagerRemote;

  setup(function() {
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    mojoApi_.resetForTest();

    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);

    mojom = chromeos.networkConfig.mojom;
    mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular, true);
  });

  teardown(function() {
    internetDetailMenu.remove();
    internetDetailMenu = null;
  });

  function getManagedProperties(type, name) {
    const result =
        OncMojo.getDefaultManagedProperties(type, name + '_guid', name);
    return result;
  }

  /** @param {boolean=} opt_isGuest */
  async function init(opt_isGuest) {
    const isGuest = !!opt_isGuest;
    loadTimeData.overrideValues({esimPolicyEnabled: true, isGuest: isGuest});

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    internetDetailMenu =
        document.createElement('settings-internet-detail-menu');

    document.body.appendChild(internetDetailMenu);
    assertTrue(!!internetDetailMenu);
    await flushAsync();
  }

  async function addEsimCellularNetwork(iccid, eid, is_managed) {
    const cellular =
        getManagedProperties(mojom.NetworkType.kCellular, 'cellular');
    cellular.typeProperties.cellular.iccid = iccid;
    cellular.typeProperties.cellular.eid = eid;
    if (is_managed) {
      cellular.source = mojom.OncSource.kDevicePolicy;
    }
    mojoApi_.setManagedPropertiesForTest(cellular);
    await flushAsync();
  }

  /**
   * Asserts that current UI element with id |elementId| is focused
   * when |deepLinkId| is search params.
   * @param {number} deepLinkId
   * @param {string} elementId
   */
  async function assertElementIsDeepLinked(deepLinkId, elementId) {
    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    params.append('settingId', deepLinkId);
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await test_util.waitAfterNextRender(internetDetailMenu);
    const actionMenu =
        internetDetailMenu.shadowRoot.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);
    const deepLinkElement = actionMenu.querySelector(`#${elementId}`);
    assertTrue(!!deepLinkElement);

    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(deepLinkElement, getDeepActiveElement());
  }

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Deep link to remove profile', async function() {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();
    await assertElementIsDeepLinked(27, 'removeBtn');
  });

  test('Deep link to rename profile', async function() {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();
    await assertElementIsDeepLinked(28, 'renameBtn');
  });

  test('Do not show triple dot when no iccid is present', async function() {
    addEsimCellularNetwork(null, '11111111111111111111111111111111');
    await init();

    let tripleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertFalse(!!tripleDot);

    addEsimCellularNetwork('100000', '11111111111111111111111111111111');

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    tripleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!tripleDot);
  });

  test('Do not show triple dot when no eid is present', async function() {
    addEsimCellularNetwork('100000', null);
    await init();

    let tripleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertFalse(!!tripleDot);

    addEsimCellularNetwork('100000', '11111111111111111111111111111111');

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    tripleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!tripleDot);
  });

  test('Do not show triple dot menu in guest mode', async function() {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init(/*opt_isGuest=*/ true);

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);
    await flushAsync();

    // Has ICCID and EID, but not shown since the user is in guest mode.
    assertFalse(!!internetDetailMenu.$$('#moreNetworkDetail'));
  });

  test('Rename menu click', async function() {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    const tripleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!tripleDot);

    tripleDot.click();
    await flushAsync();

    const actionMenu =
        internetDetailMenu.shadowRoot.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);

    const renameBtn = actionMenu.querySelector('#renameBtn');
    assertTrue(!!renameBtn);

    const renameProfilePromise = test_util.eventToPromise(
        'show-esim-profile-rename-dialog', internetDetailMenu);
    renameBtn.click();
    await Promise.all([renameProfilePromise, test_util.flushTasks()]);

    assertFalse(actionMenu.open);
  });

  test('Remove menu button click', async function() {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    const tripleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!tripleDot);

    tripleDot.click();
    await flushAsync();

    const actionMenu =
        internetDetailMenu.shadowRoot.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);

    const removeBtn = actionMenu.querySelector('#removeBtn');
    assertTrue(!!removeBtn);

    const removeProfilePromise = test_util.eventToPromise(
        'show-esim-remove-profile-dialog', internetDetailMenu);
    removeBtn.click();
    await Promise.all([removeProfilePromise, test_util.flushTasks()]);

    assertFalse(actionMenu.open);
  });

  test('Menu is disabled when inhibited', async function() {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    init();

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    const tripleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!tripleDot);
    assertFalse(tripleDot.disabled);

    internetDetailMenu.deviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      inhibitReason: mojom.InhibitReason.kConnectingToProfile,
    };
    assertTrue(tripleDot.disabled);

    internetDetailMenu.deviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      inhibitReason: mojom.InhibitReason.kNotInhibited,
    };
    assertFalse(tripleDot.disabled);
  });

  test('Menu is disabled on managed profile', async function() {
    addEsimCellularNetwork(
        '100000', '11111111111111111111111111111111', /*is_managed=*/ true);
    init();

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    const tripleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!tripleDot);
    assertTrue(tripleDot.disabled);
  });

  test(
      'Esim profile name is updated when value changes in eSIM manager',
      async function() {
        const profileName = 'test profile name';
        const iccid = '100000';
        const eid = '1111111111';

        addEsimCellularNetwork(iccid, eid);
        init();
        await flushAsync();
        const tripleDot = internetDetailMenu.$$('#moreNetworkDetail');
        assertTrue(!!tripleDot);
        assertFalse(tripleDot.disabled);

        // Change esim profile name.
        const cellular =
            getManagedProperties(mojom.NetworkType.kCellular, 'cellular');
        cellular.typeProperties.cellular.iccid = iccid;
        cellular.typeProperties.cellular.eid = eid;
        cellular.name.activeValue = profileName;
        mojoApi_.setManagedPropertiesForTest(cellular);
        await flushAsync();

        // Trigger change in esim manager listener
        eSimManagerRemote.notifyProfileChangedForTest(null);
        await flushAsync();

        tripleDot.click();
        await flushAsync();

        const actionMenu =
            internetDetailMenu.shadowRoot.querySelector('cr-action-menu');
        assertTrue(!!actionMenu);
        assertTrue(actionMenu.open);

        const renameBtn = actionMenu.querySelector('#renameBtn');
        assertTrue(!!renameBtn);

        const renameProfilePromise = test_util.eventToPromise(
            'show-esim-profile-rename-dialog', internetDetailMenu);
        renameBtn.click();
        const event = await renameProfilePromise;
        assertEquals(profileName, event.detail.networkState.name);
      });

  test('Network state is null if no profile is found', async function() {
    const getTrippleDot = () => {
      return internetDetailMenu.$$('#moreNetworkDetail');
    };
    addEsimCellularNetwork('1', '1');
    await init();
    assertTrue(!!getTrippleDot());

    // Remove current eSIM profile.
    mojoApi_.resetForTest();
    await flushAsync();

    // Trigger change in esim manager listener
    eSimManagerRemote.notifyProfileChangedForTest(null);
    await flushAsync();

    assertFalse(!!getTrippleDot());
  });
});
