// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {Router, Route, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {eventToPromise} from 'chrome://test/test_util.js';
// #import {assertTrue, assertEquals, assertFalse} from '../../../chai_assert.js';
// #import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
// #import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
// clang-format on

suite('OsBluetoothDeviceDetailPageTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothDeviceDetailSubpageElement|undefined} */
  let bluetoothDeviceDetailPage;

  /** @type {!chromeos.bluetoothConfig.mojom} */
  let mojom;

  /**
   * @type {!chromeos.bluetoothConfig.mojom.SystemPropertiesObserverInterface}
   */
  let propertiesObserver;

  setup(function() {
    mojom = chromeos.bluetoothConfig.mojom;
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
  });

  function init() {
    bluetoothDeviceDetailPage =
        document.createElement('os-settings-bluetooth-device-detail-subpage');
    document.body.appendChild(bluetoothDeviceDetailPage);
    Polymer.dom.flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override
       * @param {!chromeos.bluetoothConfig.mojom.BluetoothSystemProperties}
       *     properties
       */
      onPropertiesUpdated(properties) {
        bluetoothDeviceDetailPage.systemProperties = properties;
      },
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
    Polymer.dom.flush();
  }

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise((resolve) => setTimeout(resolve));
  }

  teardown(function() {
    bluetoothDeviceDetailPage.remove();
    bluetoothDeviceDetailPage = null;
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Show change settings row, and navigate to subpages', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const getChangeMouseSettings = () =>
        bluetoothDeviceDetailPage.$$('#changeMouseSettings');
    const getChangeKeyboardSettings = () =>
        bluetoothDeviceDetailPage.$$('#changeKeyboardSettings');

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushAsync();

    assertFalse(!!getChangeMouseSettings());
    assertFalse(!!getChangeKeyboardSettings());

    let params = new URLSearchParams();
    params.append('id', '12//345&6789');
    settings.Router.getInstance().navigateTo(
        settings.routes.BLUETOOTH_DEVICE_DETAIL, params);

    await flushAsync();
    assertTrue(!!getChangeMouseSettings());
    assertFalse(!!getChangeKeyboardSettings());
    assertEquals(
        bluetoothDeviceDetailPage.i18n(
            'bluetoothDeviceDetailChangeDeviceSettingsMouse'),
        getChangeMouseSettings().label);

    getChangeMouseSettings().click();
    await flushAsync();

    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.POINTERS);

    device1.deviceProperties.deviceType = mojom.DeviceType.kKeyboard;
    bluetoothConfig.updatePairedDevice(device1);
    await flushAsync();

    params = new URLSearchParams();
    params.append('id', '12//345&6789');
    settings.Router.getInstance().navigateTo(
        settings.routes.BLUETOOTH_DEVICE_DETAIL, params);

    await flushAsync();
    assertFalse(!!getChangeMouseSettings());
    assertTrue(!!getChangeKeyboardSettings());

    getChangeKeyboardSettings().click();
    await flushAsync();

    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.KEYBOARD);
  });

  test('Device becomes unavailable while viewing page.', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const windowPopstatePromise = test_util.eventToPromise('popstate', window);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12345/6789&',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    const device2 = createDefaultBluetoothDevice(
        /*id=*/ '987654321',
        /*publicName=*/ 'MX 3',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device2',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToPairedDeviceList([device1, device2]);
    await flushAsync();

    const params = new URLSearchParams();
    params.append('id', '12345/6789&');
    settings.Router.getInstance().navigateTo(
        settings.routes.BLUETOOTH_DEVICE_DETAIL, params);

    await flushAsync();
    assertEquals('device1', bluetoothDeviceDetailPage.parentNode.pageTitle);
    bluetoothConfig.removePairedDevice(device1);
    await windowPopstatePromise;
  });

  test('Device UI states test', async function() {
    init();
    const getBluetoothStatusIcon = () =>
        bluetoothDeviceDetailPage.$$('#statusIcon');
    const getBluetoothStateText = () =>
        bluetoothDeviceDetailPage.$$('#bluetoothStateText');
    const getBluetoothForgetBtn = () =>
        bluetoothDeviceDetailPage.$$('#forgetBtn');
    const getBluetoothStateBtn = () =>
        bluetoothDeviceDetailPage.$$('#connectDisconnectBtn');
    const getBluetoothDeviceNameLabel = () =>
        bluetoothDeviceDetailPage.$$('#bluetoothDeviceNameLabel');
    const getBluetoothDeviceBatteryInfo = () =>
        bluetoothDeviceDetailPage.$$('#batteryInfo');

    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    assertTrue(!!getBluetoothStatusIcon());
    assertTrue(!!getBluetoothStateText());
    assertTrue(!!getBluetoothForgetBtn());
    assertTrue(!!getBluetoothDeviceNameLabel());
    assertFalse(!!getBluetoothStateBtn());
    assertFalse(!!getBluetoothDeviceBatteryInfo());

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '123456789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_udioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kHeadset);

    device1.deviceProperties.batteryInfo = {
      defaultProperties: {batteryPercentage: 90}
    };
    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushAsync();

    const params = new URLSearchParams();
    params.append('id', '123456789');
    settings.Router.getInstance().navigateTo(
        settings.routes.BLUETOOTH_DEVICE_DETAIL, params);
    await flushAsync();

    // Simulate connected state and audio capable.
    assertTrue(!!getBluetoothStateBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        getBluetoothStateText().textContent.trim());
    assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    assertEquals('device1', getBluetoothDeviceNameLabel().textContent.trim());
    assertEquals(
        'os-settings:bluetooth-connected', getBluetoothStatusIcon().icon);
    assertTrue(!!getBluetoothDeviceBatteryInfo());

    // Simulate disconnected state and not audio capable.
    device1.deviceProperties.connectionState =
        mojom.DeviceConnectionState.kNotConnected;
    device1.deviceProperties.audioCapability =
        mojom.AudioOutputCapability.kNotCapableOfAudioOutput;
    device1.deviceProperties.batteryInfo = {defaultProperties: null};
    bluetoothConfig.updatePairedDevice(device1);
    await flushAsync();

    assertFalse(!!getBluetoothStateBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        getBluetoothStateText().textContent.trim());
    assertFalse(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    assertEquals(
        'os-settings:bluetooth-disabled', getBluetoothStatusIcon().icon);
    assertFalse(!!getBluetoothDeviceBatteryInfo());
  });

  test(
      'Change device dialog is shown after change name button click',
      async function() {
        init();
        bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

        const getChangeDeviceNameDialog = () =>
            bluetoothDeviceDetailPage.$$('#changeDeviceNameDialog');

        const device1 = createDefaultBluetoothDevice(
            /*id=*/ '12//345&6789',
            /*publicName=*/ 'BeatsX',
            /*connectionState=*/
            chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
            /*opt_nickname=*/ 'device1',
            /*opt_audioCapability=*/
            mojom.AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ mojom.DeviceType.kMouse);

        bluetoothConfig.appendToPairedDeviceList([device1]);
        await flushAsync();

        const params = new URLSearchParams();
        params.append('id', '12//345&6789');
        settings.Router.getInstance().navigateTo(
            settings.routes.BLUETOOTH_DEVICE_DETAIL, params);

        await flushAsync();

        assertFalse(!!getChangeDeviceNameDialog());

        const changeNameBtn = bluetoothDeviceDetailPage.$$('#changeNameBtn');
        assertTrue(!!changeNameBtn);
        changeNameBtn.click();

        await flushAsync();
        assertTrue(!!getChangeDeviceNameDialog());
      });

  test('Landing on page while device is still connecting', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const getBluetoothConnectDisconnectBtn = () =>
        bluetoothDeviceDetailPage.$$('#connectDisconnectBtn');
    const getBluetoothStateText = () =>
        bluetoothDeviceDetailPage.$$('#bluetoothStateText');
    const getConnectionFailedText = () =>
        bluetoothDeviceDetailPage.$$('#connectionFailed');

    const id = '12//345&6789';

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ id,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnecting,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushAsync();

    const params = new URLSearchParams();
    params.append('id', id);
    settings.Router.getInstance().navigateTo(
        settings.routes.BLUETOOTH_DEVICE_DETAIL, params);

    await flushAsync();
    await flushAsync();
    assertTrue(!!getBluetoothConnectDisconnectBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothConnecting'),
        getBluetoothStateText().textContent.trim());
    assertFalse(!!getConnectionFailedText());
    assertTrue(getBluetoothConnectDisconnectBtn().disabled);
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'),
        getBluetoothConnectDisconnectBtn().textContent.trim());
  });

  test('Connect/Disconnect/forget states and error message', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const windowPopstatePromise = test_util.eventToPromise('popstate', window);

    const getBluetoothForgetBtn = () =>
        bluetoothDeviceDetailPage.$$('#forgetBtn');
    const getBluetoothConnectDisconnectBtn = () =>
        bluetoothDeviceDetailPage.$$('#connectDisconnectBtn');

    const getBluetoothStateText = () =>
        bluetoothDeviceDetailPage.$$('#bluetoothStateText');
    const getConnectionFailedText = () =>
        bluetoothDeviceDetailPage.$$('#connectionFailed');

    const assertUIState =
        (isShowingConnectionFailed, isConnectDisconnectBtnDisabled,
         bluetoothStateText, connectDisconnectBtnText) => {
          assertEquals(!!getConnectionFailedText(), isShowingConnectionFailed);
          assertEquals(
              getBluetoothConnectDisconnectBtn().disabled,
              isConnectDisconnectBtnDisabled);
          assertEquals(
              getBluetoothStateText().textContent.trim(), bluetoothStateText);
          assertEquals(
              getBluetoothConnectDisconnectBtn().textContent.trim(),
              connectDisconnectBtnText);
        };

    const id = '12345/6789&';
    const device1 = createDefaultBluetoothDevice(
        id,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kNotConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    device1.deviceProperties.batteryInfo = {
      defaultProperties: {batteryPercentage: 90}
    };

    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushAsync();

    const params = new URLSearchParams();
    params.append('id', id);
    settings.Router.getInstance().navigateTo(
        settings.routes.BLUETOOTH_DEVICE_DETAIL, params);

    await flushAsync();
    assertTrue(!!getBluetoothConnectDisconnectBtn());
    // Disconnected without error.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Try to connect.
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
    // Connecting.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ true,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnecting'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));
    bluetoothConfig.completeConnect(/*success=*/ false);

    // Connection fails.
    await flushAsync();
    // Disconnected with error.
    assertUIState(
        /*isShowingConnectionFailed=*/ true,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Try to connect with success.
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
    bluetoothConfig.completeConnect(/*success=*/ true);
    // Connection success.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDisconnect'));

    // Disconnect device and check that connection error is not shown.
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
    bluetoothConfig.completeDisconnect(/*success=*/ true);
    await flushAsync();
    // Disconnected without error.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Try to connect with error.
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
    bluetoothConfig.completeConnect(/*success=*/ false);
    await flushAsync();
    // Disconnected with error.
    assertUIState(
        /*isShowingConnectionFailed=*/ true,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Device automatically reconnects without calling connect. This
    // can happen if connection failure was because device was turned off
    // and is turned on. We expect connection error text to not show when
    // disconnected.
    device1.deviceProperties.connectionState =
        mojom.DeviceConnectionState.kConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushAsync();
    // Connection success.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDisconnect'));

    // Disconnect device and check that connection error is not shown.
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
    bluetoothConfig.completeDisconnect(/*success=*/ true);
    await flushAsync();
    // Disconnected without error.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));


    // Retry connection with success.
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
    // Connecting.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ true,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnecting'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));
    bluetoothConfig.completeConnect(/*success=*/ true);
    await flushAsync();
    // Connection success.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDisconnect'));

    // Forget device.
    getBluetoothForgetBtn().click();
    await flushAsync();
    bluetoothConfig.completeForget(/*success=*/ true);
    await windowPopstatePromise;

    // Device should be null after navigating backward.
    assertFalse(!!bluetoothDeviceDetailPage.getDeviceForTest());
  });
});
