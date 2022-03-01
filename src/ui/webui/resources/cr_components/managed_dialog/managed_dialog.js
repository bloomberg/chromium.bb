// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'managed-dialog' is a dialog that is displayed when a user
 * interact with some UI features which are managed by the user's organization.
 */
import '../../cr_elements/cr_button/cr_button.m.js';
import '../../cr_elements/cr_dialog/cr_dialog.m.js';
import '../../cr_elements/icons.m.js';
import '../../cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, I18nBehaviorInterface} from '../../js/i18n_behavior.m.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ManagedDialogElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class ManagedDialogElement extends ManagedDialogElementBase {
  static get is() {
    return 'managed-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Managed dialog title text. */
      title: String,

      /** Managed dialog body text. */
      body: String,
    };
  }

  /** @private */
  onOkClick_() {
    this.$.dialog.close();
  }
}

customElements.define(ManagedDialogElement.is, ManagedDialogElement);
