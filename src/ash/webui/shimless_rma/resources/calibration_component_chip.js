// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './icons.js';

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'calibration-component-chip' represents a single component chip that reports
 * status of last calibration attempt and can be marked to skip.
 */

export class CalibrationComponentChipElement extends PolymerElement {
  static get is() {
    return 'calibration-component-chip';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {boolean} */
      disabled: {
        type: Boolean,
        value: false,
      },

      /** @type {boolean} */
      skip: {
        notify: true,
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      /** @type {boolean} */
      completed: {
        notify: true,
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      /** @type {boolean} */
      failed: {
        notify: true,
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      /** @type {string} */
      componentName: {type: String, value: ''},

      /** @type {string} */
      componentStatus: {type: String, value: ''}
    };
  }

  /** @protected */
  onComponentButtonClicked_() {
    this.skip = !this.skip;
  }

  click() {
    this.onComponentButtonClicked_();
  }
};

customElements.define(
    CalibrationComponentChipElement.is, CalibrationComponentChipElement);
