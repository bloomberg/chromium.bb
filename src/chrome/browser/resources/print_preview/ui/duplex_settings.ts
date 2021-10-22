// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import 'chrome://resources/polymer/v3_0/iron-meta/iron-meta.js';
import './icons.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import {IronMeta} from 'chrome://resources/polymer/v3_0/iron-meta/iron-meta.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DuplexMode} from '../data/model.js';
import {getSelectDropdownBackground} from '../print_preview_utils.js';

import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

export interface PrintPreviewDuplexSettingsElement {
  $: {
    duplex: CrCheckboxElement,
  };
}

const PrintPreviewDuplexSettingsElementBase =
    SettingsMixin(SelectMixin(PolymerElement));

export class PrintPreviewDuplexSettingsElement extends
    PrintPreviewDuplexSettingsElementBase {
  static get is() {
    return 'print-preview-duplex-settings';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      dark: Boolean,

      disabled: Boolean,

      /**
       * Mirroring the enum so that it can be used from HTML bindings.
       */
      duplexValueEnum_: {
        type: Object,
        value: DuplexMode,
      },
    };
  }

  static get observers() {
    return [
      'onDuplexSettingChange_(settings.duplex.*)',
      'onDuplexTypeChange_(settings.duplexShortEdge.*)',
    ];
  }

  dark: boolean;
  disabled: boolean;
  private meta_: IronMeta;

  constructor() {
    super();

    this.meta_ = new IronMeta({type: 'iconset', value: undefined});
  }

  private onDuplexSettingChange_() {
    this.$.duplex.checked = this.getSettingValue('duplex');
  }

  private onDuplexTypeChange_() {
    this.selectedValue = this.getSettingValue('duplexShortEdge') ?
        DuplexMode.SHORT_EDGE.toString() :
        DuplexMode.LONG_EDGE.toString();
  }

  private onCheckboxChange_() {
    this.setSetting('duplex', this.$.duplex.checked);
  }

  onProcessSelectChange(value: string) {
    this.setSetting(
        'duplexShortEdge', value === DuplexMode.SHORT_EDGE.toString());
  }

  /**
   * @return Whether to expand the collapse for the dropdown.
   */
  private getOpenCollapse_(): boolean {
    return this.getSetting('duplexShortEdge').available &&
        (this.getSettingValue('duplex') as boolean);
  }

  /**
   * @param managed Whether the setting is managed by policy.
   * @param disabled value of this.disabled
   * @return Whether the controls should be disabled.
   */
  private getDisabled_(managed: boolean, disabled: boolean): boolean {
    return managed || disabled;
  }

  /**
   * @return An inline svg corresponding to |icon| and the image for
   *     the dropdown arrow.
   */
  private getBackgroundImages_(): string {
    const icon =
        this.getSettingValue('duplexShortEdge') ? 'short-edge' : 'long-edge';
    const iconset = this.meta_.byKey('print-preview');
    return getSelectDropdownBackground(iconset, icon, this);
  }
}

customElements.define(
    PrintPreviewDuplexSettingsElement.is, PrintPreviewDuplexSettingsElement);
