// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '../i18n_setup.js';
import '../settings_shared_css.js';

import {assert} from '//resources/js/assert.m.js';
import {I18nMixin} from '//resources/js/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionControlBrowserProxyImpl} from '../extension_control_browser_proxy.js';

import {getTemplate} from './extension_controlled_indicator.html.js';

const ExtensionControlledIndicatorElementBase = I18nMixin(PolymerElement);

export class ExtensionControlledIndicatorElement extends
    ExtensionControlledIndicatorElementBase {
  static get is() {
    return 'extension-controlled-indicator';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      extensionCanBeDisabled: Boolean,
      extensionId: String,
      extensionName: String,
    };
  }

  extensionCanBeDisabled: boolean;
  extensionId: string;
  extensionName: string;

  private getLabel_(): string {
    if (this.extensionId === undefined || this.extensionName === undefined) {
      return '';
    }

    const manageUrl = 'chrome://extensions/?id=' + this.extensionId;
    return this.i18nAdvanced('controlledByExtension', {
      substitutions:
          ['<a href="' + manageUrl + '" target="_blank">' + this.extensionName +
           '</a>'],
    });
  }

  private onDisableTap_() {
    assert(this.extensionCanBeDisabled);
    ExtensionControlBrowserProxyImpl.getInstance().disableExtension(
        assert(this.extensionId));
    this.dispatchEvent(
        new CustomEvent('extension-disable', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extension-controlled-indicator': ExtensionControlledIndicatorElement;
  }
}

customElements.define(
    ExtensionControlledIndicatorElement.is,
    ExtensionControlledIndicatorElement);
