// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'upi-id-list-entry' is a UPI ID row to be shown in
 * the settings page. https://en.wikipedia.org/wiki/Unified_Payments_Interface
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../i18n_setup.js';
import '../settings_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsUpiIdListEntryElement extends PolymerElement {
  static get is() {
    return 'settings-upi-id-list-entry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** A saved UPI ID. */
      upiId: String,
    };
  }
}

customElements.define(
    SettingsUpiIdListEntryElement.is, SettingsUpiIdListEntryElement);
