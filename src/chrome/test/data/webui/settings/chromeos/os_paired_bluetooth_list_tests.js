// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue, assertEquals} from '../../../chai_assert.js';
// #import {eventToPromise} from 'chrome://test/test_util.js';
// #import {createDefaultBluetoothDevice} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
// clang-format on

suite('OsPairedBluetoothListTest', function() {
  /** @type {!SettingsPairedBluetoothListElement|undefined} */
  let pairedBluetoothList;

  setup(function() {
    pairedBluetoothList =
        document.createElement('os-settings-paired-bluetooth-list');
    document.body.appendChild(pairedBluetoothList);
    Polymer.dom.flush();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Base Test', function() {
    const list = pairedBluetoothList.shadowRoot.querySelector('iron-list');
    assertTrue(!!list);
  });

  test('Device list change renders items correctly', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected);

    pairedBluetoothList.devices = [device, device, device];
    await flushAsync();

    const getListItems = () => {
      return pairedBluetoothList.shadowRoot.querySelectorAll(
          'os-settings-paired-bluetooth-list-item');
    };
    assertEquals(getListItems().length, 3);

    const ironResizePromise =
        test_util.eventToPromise('iron-resize', pairedBluetoothList);
    pairedBluetoothList.devices = [device, device, device, device, device];

    await ironResizePromise;
    Polymer.dom.flush();
    assertEquals(getListItems().length, 5);
  });
});
