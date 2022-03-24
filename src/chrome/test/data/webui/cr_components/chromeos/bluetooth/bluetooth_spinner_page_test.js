// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothSpinnerPageElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_spinner_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from '../../../chai_assert.js';

// clang-format on

suite('CrComponentsBluetoothSpinnerPageTest', function() {
  /** @type {?SettingsBluetoothSpinnerPageElement} */
  let bluetoothSpinnerPage;

  setup(function() {
    bluetoothSpinnerPage = /** @type {?SettingsBluetoothSpinnerPageElement} */ (
        document.createElement('bluetooth-spinner-page'));
    document.body.appendChild(bluetoothSpinnerPage);
    flush();
  });

  test('Spinner is shown', function() {
    const spinner =
        bluetoothSpinnerPage.shadowRoot.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);
  });
});
