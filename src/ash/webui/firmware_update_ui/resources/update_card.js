// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import '/file_path.mojom-lite.js';
import './firmware_shared_css.js';
import './firmware_shared_fonts.js';
import './mojom/firmware_update.mojom-lite.js';
import './strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FirmwareUpdate, UpdatePriority} from './firmware_update_types.js';
import {mojoString16ToString} from './mojo_utils.js';

/**
 * @fileoverview
 * 'update-card' displays information about a peripheral update.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const UpdateCardElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class UpdateCardElement extends UpdateCardElementBase {
  static get is() {
    return 'update-card';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!FirmwareUpdate} */
      update: {
        type: Object,
      },

      disabled: {
        type: Boolean,
      },
    };
  }

  /**
   * @protected
   * @return {boolean}
   */
  isCriticalUpdate_() {
    return this.update.priority === UpdatePriority.kCritical;
  }

  /** @protected */
  onUpdateButtonClicked_() {
    this.dispatchEvent(new CustomEvent('open-update-dialog', {
      bubbles: true,
      composed: true,
      detail: {update: this.update, inflight: false}
    }));
  }

  /**
   * @protected
   * @return {string}
   */
  computeVersionText_() {
    if (!this.update.deviceVersion) {
      return '';
    }

    return this.i18n('versionText', this.update.deviceVersion);
  }

  /**
   * @protected
   * @return {string}
   */
  computeDeviceName_() {
    return mojoString16ToString(this.update.deviceName);
  }

  /**
   * @protected
   * @return {string}
   */
  computeDeviceDescription_() {
    return mojoString16ToString(this.update.deviceDescription);
  }
}

customElements.define(UpdateCardElement.is, UpdateCardElement);
