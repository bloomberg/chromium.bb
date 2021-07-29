// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'address-remove-confirmation-dialog' is the dialog that allows
 * removing a saved address.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


/** @polymer */
export class SettingsAddressRemoveConfirmationDialogElement extends
    PolymerElement {
  static get is() {
    return 'settings-address-remove-confirmation-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @return {boolean} Whether the user confirmed the dialog. */
  wasConfirmed() {
    return /** @type {!CrDialogElement} */ (this.$.dialog)
               .getNative()
               .returnValue === 'success';
  }

  /** @private */
  onRemoveClick() {
    this.$.dialog.close();
  }

  /** @private */
  onCancelClick() {
    this.$.dialog.cancel();
  }
}

customElements.define(
    SettingsAddressRemoveConfirmationDialogElement.is,
    SettingsAddressRemoveConfirmationDialogElement);
