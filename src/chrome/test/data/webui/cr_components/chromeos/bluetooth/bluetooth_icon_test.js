// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {SettingsBluetoothIconElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from '../../../chai_assert.js';
import {createDefaultBluetoothDevice} from './fake_bluetooth_config.js';
// clang-format on

const mojom = chromeos.bluetoothConfig.mojom;

suite('CrComponentsBluetoothIconTest', function() {
  /** @type {?SettingsBluetoothIconElement} */
  let bluetoothIcon;

  async function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    bluetoothIcon = /** @type {?SettingsBluetoothIconElement} */ (
        document.createElement('bluetooth-icon'));
    document.body.appendChild(bluetoothIcon);
    assertTrue(!!bluetoothIcon);
    flush();
  });

  test('Correct icon is shown', async function() {
    const getDeviceIcon = () =>
        bluetoothIcon.shadowRoot.querySelector('#deviceTypeIcon');
    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse);

    bluetoothIcon.device = device.deviceProperties;
    await flushAsync();

    assertEquals(getDeviceIcon().icon, 'bluetooth:mouse');

    device.deviceProperties.deviceType = mojom.DeviceType.kUnknown;

    bluetoothIcon.device =
        /**
           @type {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
         */
        (Object.assign({}, device.deviceProperties));
    await flushAsync();

    assertEquals(getDeviceIcon().icon, 'bluetooth:default');
  });
});