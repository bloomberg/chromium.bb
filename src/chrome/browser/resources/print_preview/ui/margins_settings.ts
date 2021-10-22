// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select_css.m.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MarginsType} from '../data/margins.js';
import {State} from '../data/state.js';

import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

const PrintPreviewMarginsSettingsElementBase =
    SettingsMixin(SelectMixin(PolymerElement));

export class PrintPreviewMarginsSettingsElement extends
    PrintPreviewMarginsSettingsElementBase {
  static get is() {
    return 'print-preview-margins-settings';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        observer: 'updateMarginsDisabled_',
      },

      /** @type {!State} */
      state: {
        type: Number,
        observer: 'onStateChange_',
      },

      marginsDisabled_: Boolean,

      /** Mirroring the enum so that it can be used from HTML bindings. */
      marginsTypeEnum_: {
        type: Object,
        value: MarginsType,
      }
    };
  }

  static get observers() {
    return [
      'onMarginsSettingChange_(settings.margins.value)',
      'onMediaSizeOrLayoutChange_(' +
          'settings.mediaSize.value, settings.layout.value)',
      'onPagesPerSheetSettingChange_(settings.pagesPerSheet.value)'

    ];
  }

  disabled: boolean;
  state: State;
  private marginsDisabled_: boolean;
  private loaded_: boolean = false;

  private onStateChange_() {
    if (this.state === State.READY) {
      this.loaded_ = true;
    }
  }

  private onMediaSizeOrLayoutChange_() {
    if (this.loaded_ &&
        this.getSetting('margins').value === MarginsType.CUSTOM) {
      this.setSetting('margins', MarginsType.DEFAULT);
    }
  }

  /**
   * @param newValue The new value of the pages per sheet setting.
   */
  private onPagesPerSheetSettingChange_(newValue: number) {
    if (newValue > 1) {
      this.setSetting('margins', MarginsType.DEFAULT);
    }
    this.updateMarginsDisabled_();
  }

  /** @param newValue The new value of the margins setting. */
  private onMarginsSettingChange_(newValue: MarginsType) {
    this.selectedValue = newValue.toString();
  }

  onProcessSelectChange(value: string) {
    this.setSetting('margins', parseInt(value, 10));
  }

  private updateMarginsDisabled_() {
    this.marginsDisabled_ =
        (this.getSettingValue('pagesPerSheet') as number) > 1 || this.disabled;
  }
}

customElements.define(
    PrintPreviewMarginsSettingsElement.is, PrintPreviewMarginsSettingsElement);
