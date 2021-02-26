// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SelectBehavior} from './select_behavior.js';

/**
 * @fileoverview
 * 'file-type-select' displays the available file types in a dropdown.
 */
Polymer({
  is: 'file-type-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  properties: {
    /** @type {string} */
    selectedFileType: {
      type: String,
      notify: true,
    },
  },

  /** @override */
  created() {
    // The dropdown always contains one option per FileType.
    this.onNumOptionsChange(chromeos.scanning.mojom.FileType.MAX_VALUE + 1);
  },
});
