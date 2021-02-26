// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './battery_status_card.js';
import './cpu_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './memory_card.js';
import './overview_card.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SystemDataProviderInterface, SystemInfo} from './diagnostics_types.js'
import {getSystemDataProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'diagnostics-app' is the main page for viewing telemetric system information
 * and running diagnostic tests.
 */
Polymer({
  is: 'diagnostics-app',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  properties: {
    /** @private */
    showBatteryStatusCard_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.fetchSystemInfo_();
  },

  /** @private */
  fetchSystemInfo_() {
    this.systemDataProvider_.getSystemInfo().then(
        this.onSystemInfoReceived_.bind(this));
  },

  /**
   * @param {{systemInfo: !SystemInfo}} result
   * @private
   */
  onSystemInfoReceived_(result) {
    this.showBatteryStatusCard_ =
        result.systemInfo.deviceCapabilities.hasBattery;
  },

});