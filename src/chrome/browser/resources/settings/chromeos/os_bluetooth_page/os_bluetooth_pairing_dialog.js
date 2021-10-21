// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element for displaying Bluetooth pairing dialog.
 */
import 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_ui.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsBluetoothPairingDialogElement extends PolymerElement {
  static get is() {
    return 'os-settings-bluetooth-pairing-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /**
   * @param {!Event} e
   * @private
   */
  closeDialog_(e) {
    this.$.dialog.close();
    e.stopPropagation();
  }
}

customElements.define(
    SettingsBluetoothPairingDialogElement.is,
    SettingsBluetoothPairingDialogElement);
