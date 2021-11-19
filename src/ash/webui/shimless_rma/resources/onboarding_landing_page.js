// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {HardwareVerificationStatusObserverInterface, HardwareVerificationStatusObserverReceiver, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'onboarding-landing-page' is the main landing page for the shimless rma
 * process.
 */
export class OnboardingLandingPage extends PolymerElement {
  static get is() {
    return 'onboarding-landing-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected */
      verificationMessage_: {
        type: String,
        // TODO(gavindodd): i18n string
        value: 'Validating components...',
      },

      /**
       * Error code from rmad service, not i18n.
       * @protected
       */
      errorMessage_: {
        type: String,
        value: '',
      },

      /** @protected */
      verificationInProgress_: {
        type: Boolean,
        value: true,
      },

      /**
       * isCompliant_ is not valid until verificationInProgress_ is false.
       * @protected
       */
      isCompliant_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /** @protected {?HardwareVerificationStatusObserverReceiver} */
    this.hwVerificationObserverReceiver_ =
        new HardwareVerificationStatusObserverReceiver(
            /**
             * @type {!HardwareVerificationStatusObserverInterface}
             */
            (this));

    this.shimlessRmaService_.observeHardwareVerificationStatus(
        this.hwVerificationObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** @override */
  ready() {
    super.ready();
  }

  /** @return {!Promise<StateResult>} */
  onNextButtonClick() {
    if (!this.verificationInProgress_) {
      return this.shimlessRmaService_.beginFinalization();
    }

    return Promise.reject(new Error('Hardware verification is not complete.'));
  }

  /**
   * @protected
   * @return {string}
   */
  getVerificationIcon_() {
    return this.isCompliant_ ? 'shimless-icon:check' : 'shimless-icon:warning';
  }

  /**
   * Implements
   * HardwareVerificationStatusObserver.onHardwareVerificationResult()
   * @param {boolean} isCompliant
   * @param {string} errorMessage
   */
  onHardwareVerificationResult(isCompliant, errorMessage) {
    // TODO(gavindodd): i18n strings
    if (isCompliant) {
      this.verificationMessage_ = 'Components are installed correctly.';
    } else {
      this.verificationMessage_ =
          'Components could not be validated. If you are certain you are ' +
          'using a qualified component updating Chrome OS may resolve this ' +
          'issue.';
      this.errorMessage_ = errorMessage;
    }
    this.isCompliant_ = isCompliant;
    this.verificationInProgress_ = false;
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
  }
}

customElements.define(OnboardingLandingPage.is, OnboardingLandingPage);
