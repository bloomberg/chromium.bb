// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './advanced_settings_dialog.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {Settings} from '../data/model.js';

interface PrintPreviewAdvancedOptionsSettingsElement {
  $: {
    button: CrButtonElement,
  }
}

class PrintPreviewAdvancedOptionsSettingsElement extends PolymerElement {
  static get is() {
    return 'print-preview-advanced-options-settings';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: Boolean,
      destination: Object,
      settings: Object,

      showAdvancedDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  disabled: boolean;
  destination: Destination;
  settings: Settings;
  private showAdvancedDialog_: boolean;

  private onButtonClick_() {
    this.showAdvancedDialog_ = true;
  }

  private onDialogClose_() {
    this.showAdvancedDialog_ = false;
    this.$.button.focus();
  }
}

customElements.define(
    PrintPreviewAdvancedOptionsSettingsElement.is,
    PrintPreviewAdvancedOptionsSettingsElement);
