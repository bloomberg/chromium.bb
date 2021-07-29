// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-history-deletion-dialog' is a dialog that is
 * optionally shown inside settings-clear-browsing-data-dialog after deleting
 * browsing history. It informs the user about the existence of other forms
 * of browsing history in their account.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../settings_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsHistoryDeletionDialogElement extends PolymerElement {
  static get is() {
    return 'settings-history-deletion-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /**
   * Click handler for the "OK" button.
   * @private
   */
  onOkClick_() {
    this.$.dialog.close();
  }
}

customElements.define(
    SettingsHistoryDeletionDialogElement.is,
    SettingsHistoryDeletionDialogElement);
