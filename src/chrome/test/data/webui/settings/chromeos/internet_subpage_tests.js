// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {eventToPromise, flushTasks, waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

suite('InternetSubpage', function() {
  /** @type {?SettingsInternetSubpageElement} */
  let internetSubpage = null;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      internetAddConnection: 'internetAddConnection',
      internetAddConnectionExpandA11yLabel:
          'internetAddConnectionExpandA11yLabel',
      internetAddConnectionNotAllowed: 'internetAddConnectionNotAllowed',
      internetAddThirdPartyVPN: 'internetAddThirdPartyVPN',
      internetAddVPN: 'internetAddVPN',
      internetAddWiFi: 'internetAddWiFi',
      internetDetailPageTitle: 'internetDetailPageTitle',
      internetKnownNetworksPageTitle: 'internetKnownNetworksPageTitle',
    });

    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function setNetworksForTest(type, networks) {
    mojoApi_.resetForTest();
    mojoApi_.setNetworkTypeEnabledState(type, true);
    mojoApi_.addNetworksForTest(networks);
    internetSubpage.defaultNetwork = networks[0];
    internetSubpage.deviceState = mojoApi_.getDeviceStateForTest(type);
  }

  function setCellularNetworks() {
    const mojom = chromeos.networkConfig.mojom;
    mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kTether);
    setNetworksForTest(mojom.NetworkType.kCellular, [
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kCellular, 'cellular1'),
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether2'),
    ]);
    internetSubpage.tetherDeviceState = {
      type: mojom.NetworkType.kTether,
      deviceState: mojom.DeviceStateType.kEnabled
    };
  }

  function initSubpage(isUpdatedCellularUiEnabled) {
    if (isUpdatedCellularUiEnabled !== undefined) {
      loadTimeData.overrideValues(
          {updatedCellularActivationUi: !!isUpdatedCellularUiEnabled});
    }
    PolymerTest.clearBody();
    internetSubpage = document.createElement('settings-internet-subpage');
    assertTrue(!!internetSubpage);
    mojoApi_.resetForTest();
    document.body.appendChild(internetSubpage);
    internetSubpage.init();
    return flushAsync();
  }

  teardown(function() {
    internetSubpage.remove();
    internetSubpage = null;
    settings.Router.getInstance().resetRouteForTesting();
  });

  suite('SubPage', function() {
    test('WiFi', function() {
      initSubpage();
      const mojom = chromeos.networkConfig.mojom;
      setNetworksForTest(mojom.NetworkType.kWiFi, [
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi2'),
      ]);
      return flushAsync().then(() => {
        assertEquals(2, internetSubpage.networkStateList_.length);
        const toggle = internetSubpage.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = internetSubpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
      });
    });

    test('Deep link to WiFi on/off toggle', async () => {
      initSubpage();
      const mojom = chromeos.networkConfig.mojom;
      setNetworksForTest(mojom.NetworkType.kWiFi, [
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi2'),
      ]);

      const params = new URLSearchParams;
      params.append('settingId', '4');
      settings.Router.getInstance().navigateTo(
          settings.routes.INTERNET_NETWORKS, params);

      await flushAsync();

      const deepLinkElement = internetSubpage.$$('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Toggle WiFi should be focused for settingId=4.');
    });

    test('Tether', function() {
      initSubpage(false);
      const mojom = chromeos.networkConfig.mojom;
      setNetworksForTest(mojom.NetworkType.kTether, [
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether2'),
      ]);
      return flushAsync().then(() => {
        assertEquals(2, internetSubpage.networkStateList_.length);
        const toggle = internetSubpage.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = internetSubpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
        const tetherToggle = internetSubpage.$$('#tetherEnabledButton');
        // No separate tether toggle when Celular is not available; the
        // primary toggle enables or disables Tether in that case.
        assertFalse(!!tetherToggle);
      });
    });

    test('Deep link to tether on/off toggle w/o cellular', async () => {
      initSubpage();
      const mojom = chromeos.networkConfig.mojom;
      setNetworksForTest(mojom.NetworkType.kTether, [
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether2'),
      ]);
      internetSubpage.tetherDeviceState = {
        type: mojom.NetworkType.kTether,
        deviceState: mojom.DeviceStateType.kEnabled
      };

      const params = new URLSearchParams;
      params.append('settingId', '22');
      settings.Router.getInstance().navigateTo(
          settings.routes.INTERNET_NETWORKS, params);

      await flushAsync();

      const deepLinkElement = internetSubpage.$$('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Device enabled should be focused for settingId=22.');
    });

    test('Fire show cellular setup event on add cellular clicked', () => {
      initSubpage(true);
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular);
      setNetworksForTest(mojom.NetworkType.kCellular, [
        OncMojo.getDefaultNetworkState(
            mojom.NetworkType.kCellular, 'cellular1'),
      ]);
      internetSubpage.deviceState = {
        type: mojom.NetworkType.kCellular,
        deviceState: mojom.DeviceStateType.kEnabled
      };
      internetSubpage.globalPolicy = {
        allowOnlyPolicyNetworksToConnect: false,
      };

      return flushAsync().then(async () => {
        const addCellularButton = internetSubpage.$$('#addCellularButton');
        assertTrue(!!addCellularButton);

        const showCellularSetupPromise =
            test_util.eventToPromise('show-cellular-setup', internetSubpage);
        addCellularButton.click();
        await Promise.all([showCellularSetupPromise, test_util.flushTasks()]);
      });
    });

    test(
        'Tether plus Cellular with updatedCellularActivationUi false',
        function() {
          initSubpage(false /* isUpdatedCellularUiEnabled */);
          const mojom = chromeos.networkConfig.mojom;
          setCellularNetworks();
          return flushAsync().then(() => {
            assertEquals(3, internetSubpage.networkStateList_.length);
            const toggle = internetSubpage.$$('#deviceEnabledButton');
            assertTrue(!!toggle);
            assertFalse(toggle.disabled);
            const networkList = internetSubpage.$$('#networkList');
            assertTrue(!!networkList);
            assertEquals(3, networkList.networks.length);
            const tetherToggle = internetSubpage.$$('#tetherEnabledButton');
            assertTrue(!!tetherToggle);
            assertFalse(tetherToggle.disabled);
          });
        });

    test(
        'Tether plus Cellular with updatedCellularActivationUi true',
        function() {
          initSubpage(true /* isUpdatedCellularUiEnabled */);
          const mojom = chromeos.networkConfig.mojom;
          setCellularNetworks();
          return flushAsync().then(() => {
            assertEquals(3, internetSubpage.networkStateList_.length);
            const toggle = internetSubpage.$$('#deviceEnabledButton');
            assertTrue(!!toggle);
            assertFalse(toggle.disabled);
            const cellularNetworkList =
                internetSubpage.$$('#cellularNetworkList');
            assertTrue(!!cellularNetworkList);
            assertEquals(3, cellularNetworkList.networks.length);
            const tetherToggle = internetSubpage.$$('#tetherEnabledButton');
            assertFalse(!!tetherToggle);
          });
        });

    test('Deep link to tether on/off toggle w/ cellular', async () => {
      initSubpage(false /* isUpdatedCellularUiEnabled */);
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kTether);
      setNetworksForTest(mojom.NetworkType.kCellular, [
        OncMojo.getDefaultNetworkState(
            mojom.NetworkType.kCellular, 'cellular1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether2'),
      ]);
      internetSubpage.tetherDeviceState = {
        type: mojom.NetworkType.kTether,
        deviceState: mojom.DeviceStateType.kEnabled
      };

      const params = new URLSearchParams;
      params.append('settingId', '22');
      settings.Router.getInstance().navigateTo(
          settings.routes.INTERNET_NETWORKS, params);

      await flushAsync();

      const deepLinkElement = internetSubpage.$$('#tetherEnabledButton');
      assertTrue(!!deepLinkElement);
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Tether enabled should be focused for settingId=22.');
    });

    suite('VPN', function() {
      function initVpn() {
        addTestVpnProviders();
        addTestVpnNetworks();
      }

      function addTestVpnProviders() {
        const mojom = chromeos.networkConfig.mojom;
        internetSubpage.vpnProviders = [
          {
            type: mojom.VpnType.kExtension,
            providerId: 'extension_id1',
            providerName: 'MyExtensionVPN1',
            appId: 'extension_id1',
            lastLaunchTime: {internalValue: 0},
          },
          {
            type: mojom.VpnType.kExtension,
            providerId: 'extension_id2',
            providerName: 'MyExtensionVPN2',
            appId: 'extension_id2',
            lastLaunchTime: {internalValue: 0},
          },
          {
            type: mojom.VpnType.kArc,
            providerId: 'vpn.app.package1',
            providerName: 'MyArcVPN1',
            appId: 'arcid1',
            lastLaunchTime: {internalValue: 1},
          },
        ];
      }

      function addTestVpnNetworks() {
        const mojom = chromeos.networkConfig.mojom;
        setNetworksForTest(mojom.NetworkType.kVPN, [
          OncMojo.getDefaultNetworkState(mojom.NetworkType.kVPN, 'vpn1'),
          OncMojo.getDefaultNetworkState(mojom.NetworkType.kVPN, 'vpn2'),
          {
            guid: 'extension1_vpn1_guid',
            name: 'vpn3',
            type: mojom.NetworkType.kVPN,
            connectionState: mojom.ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: mojom.VpnType.kExtension,
                providerId: 'extension_id1',
                providerName: 'MyExntensionVPN1',
              }
            }
          },
          {
            guid: 'extension1_vpn2_guid',
            name: 'vpn4',
            type: mojom.NetworkType.kVPN,
            connectionState: mojom.ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: mojom.VpnType.kExtension,
                providerId: 'extension_id1',
                providerName: 'MyExntensionVPN1',
              }
            }
          },
          {
            guid: 'extension2_vpn1_guid',
            name: 'vpn5',
            type: mojom.NetworkType.kVPN,
            connectionState: mojom.ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: mojom.VpnType.kExtension,
                providerId: 'extension_id2',
                providerName: 'MyExntensionVPN2',
              }
            }
          },
          {
            guid: 'arc_vpn1_guid',
            name: 'vpn6',
            type: mojom.NetworkType.kVPN,
            connectionState: mojom.ConnectionStateType.kConnected,
            typeState: {
              vpn: {
                type: mojom.VpnType.kArc,
                providerId: 'vpn.app.package1',
                providerName: 'MyArcVPN1',
              }
            }
          },
          {
            guid: 'arc_vpn2_guid',
            name: 'vpn7',
            type: mojom.NetworkType.kVPN,
            connectionState: mojom.ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: mojom.VpnType.kArc,
                providerId: 'vpn.app.package1',
                providerName: 'MyArcVPN1',
              }
            }
          },
        ]);
      }

      test('should update network state list properly', function() {
        initSubpage();
        initVpn();
        return flushAsync().then(() => {
          const allNetworkLists =
              internetSubpage.shadowRoot.querySelectorAll('network-list');
          // Built-in networks + 2 extension providers + 1 arc provider = 4
          assertEquals(4, allNetworkLists.length);
          // 2 built-in networks
          assertEquals(2, allNetworkLists[0].networks.length);
          // 2 networks with extension id 'extension_id1'
          assertEquals(2, allNetworkLists[1].networks.length);
          // 1 network with extension id 'extension_id2'
          assertEquals(1, allNetworkLists[2].networks.length);
          // 1 connected network with arc id 'vpn.app.package1'
          assertEquals(1, allNetworkLists[3].networks.length);
        });
      });

      test(
          'should not show built-in VPN list when device is disabled',
          function() {
            initSubpage();
            initVpn();
            const mojom = chromeos.networkConfig.mojom;
            internetSubpage.deviceState = {
              type: mojom.NetworkType.kVPN,
              deviceState: mojom.DeviceStateType.kProhibited
            };

            return flushAsync().then(() => {
              const allNetworkLists =
                  internetSubpage.shadowRoot.querySelectorAll('network-list');
              // 2 extension providers + 1 arc provider = 3
              // No built-in networks.
              assertEquals(3, allNetworkLists.length);
              // 2 networks with extension id 'extension_id1'
              assertEquals(2, allNetworkLists[0].networks.length);
              // 1 network with extension id 'extension_id2'
              assertEquals(1, allNetworkLists[1].networks.length);
              // 1 connected network with arc id 'vpn.app.package1'
              assertEquals(1, allNetworkLists[2].networks.length);
            });
          });
    });
  });
});
