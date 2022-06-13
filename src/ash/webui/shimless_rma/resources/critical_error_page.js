// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared_css.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'critical-error-page' is displayed when an unexpected error blocks RMA from
 * continuing.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const CriticalErrorPageBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class CriticalErrorPage extends CriticalErrorPageBase {
  static get is() {
    return 'critical-error-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /** @protected */
  onRecoverFirmwareButtonClicked_() {
    // TODO(swifton): Rename the method to match the action that it performs.
    this.shimlessRmaService_.criticalErrorExitToLogin();
  }

  /** @protected */
  onRebootButtonClicked_() {
    this.shimlessRmaService_.criticalErrorReboot();
  }
}

customElements.define(CriticalErrorPage.is, CriticalErrorPage);
