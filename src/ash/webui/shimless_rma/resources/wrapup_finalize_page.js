// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {FinalizationObserverInterface, FinalizationObserverReceiver, FinalizationStatus, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/** @type {!Object<!FinalizationStatus, string>} */
const finalizationStatusTextKeys = {
  [FinalizationStatus.kInProgress]: 'finalizePageProgressText',
  [FinalizationStatus.kComplete]: 'finalizePageCompleteText',
  [FinalizationStatus.kFailedBlocking]: 'finalizePageFailedBlockingText',
  [FinalizationStatus.kFailedNonBlocking]: 'finalizePageFailedNonBlockingText',
};

/**
 * @fileoverview
 * 'wrapup-finalize-page' wait for device finalization and hardware verification
 * to be completed.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const WrapupFinalizePageBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class WrapupFinalizePage extends WrapupFinalizePageBase {
  static get is() {
    return 'wrapup-finalize-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected */
      finalizationMessage_: {
        type: String,
        value: '',
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /** @private {boolean} */
    this.finalizationComplete_ = false;
    /**
     * Receiver responsible for observing hardware write protection state.
     * @private {?FinalizationObserverReceiver}
     */
    this.finalizationObserverReceiver_ = new FinalizationObserverReceiver(
        /** @type {!FinalizationObserverInterface} */ (this));

    this.shimlessRmaService_.observeFinalizationStatus(
        this.finalizationObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * @param {!FinalizationStatus} status
   * @param {number} progress
   */
  onFinalizationUpdated(status, progress) {
    if (status === FinalizationStatus.kInProgress) {
      this.finalizationMessage_ = this.i18n(
          finalizationStatusTextKeys[status], Math.round(progress * 100));
    } else {
      this.finalizationMessage_ = this.i18n(finalizationStatusTextKeys[status]);
    }
    this.finalizationComplete_ = status === FinalizationStatus.kComplete ||
        status === FinalizationStatus.kFailedNonBlocking;
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: !this.finalizationComplete_},
        ));
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.finalizationComplete_) {
      return this.shimlessRmaService_.finalizationComplete();
    } else {
      return Promise.reject(new Error('Finalization is not complete.'));
    }
  }
}

customElements.define(WrapupFinalizePage.is, WrapupFinalizePage);
