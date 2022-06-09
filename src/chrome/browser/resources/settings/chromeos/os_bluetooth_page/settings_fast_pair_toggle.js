// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Reusable toggle that turns Fast Pair on and off.
 */

import '../../controls/settings_toggle_button.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getBluetoothConfig} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';

/** @polymer */
class SettingsFastPairToggleElement extends PolymerElement {
  static get is() {
    return 'settings-fast-pair-toggle';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }
}

customElements.define(
    SettingsFastPairToggleElement.is, SettingsFastPairToggleElement);
