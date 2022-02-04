// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {QrCode, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

// The size of each tile in pixels.
const QR_CODE_TILE_SIZE = 5;
// Amount of padding around the QR code in pixels.
const QR_CODE_PADDING = 4 * QR_CODE_TILE_SIZE;
// Styling for filled tiles in the QR code.
const QR_CODE_FILL_STYLE = '#000000';

/**
 * @fileoverview
 * 'onboarding-enter-rsu-wp-disable-code-page' asks the user for the RSU disable
 * code.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingEnterRsuWpDisableCodePageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingEnterRsuWpDisableCodePage extends
    OnboardingEnterRsuWpDisableCodePageBase {
  static get is() {
    return 'onboarding-enter-rsu-wp-disable-code-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: Boolean,

      /** @protected */
      canvasSize_: {
        type: Number,
        value: 0,
      },

      /** @protected {string} */
      rsuChallenge_: {
        type: String,
        value: '',
      },

      /** @protected */
      rsuHwid_: {
        type: String,
        value: '',
      },

      /** @protected */
      rsuCode_: {
        type: String,
        value: '',
        observer: 'onRsuCodeChanged_',
      },

      /** @protected */
      rsuInstructionsText_: {
        type: String,
        value: '',
      },

      /** @protected */
      rsuChallengeLinkText_: {
        type: String,
        value: '',
        computed: 'computeRsuChallengeLinkText_(rsuHwid_, rsuChallenge_)',
      },

      /** @protected */
      rsuCodeValidationRegex_: {
        type: String,
        value: '.{1,8}',
        readOnly: true,
      },

      /** @protected {boolean} */
      rsuCodeInvalid_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** @protected */
      rsuCodeLengthLabel_: {
        type: String,
        computed: 'computeRsuCodeLengthLabel_(rsuCode_)',
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /** @override */
  ready() {
    super.ready();
    this.getRsuChallengeAndHwid_();
    this.setRsuInstructionsText_();
  }

  /** @private */
  getRsuChallengeAndHwid_() {
    this.shimlessRmaService_.getRsuDisableWriteProtectChallenge().then(
        (result) => this.rsuChallenge_ = result.challenge);
    this.shimlessRmaService_.getRsuDisableWriteProtectHwid().then(
        (result) => {
          this.rsuHwid_ = result.hwid;
        });
    this.shimlessRmaService_.getRsuDisableWriteProtectChallengeQrCode().then(
        this.updateQrCode_.bind(this));
  }

  /**
   * @private
   * @param {?{qrCode: QrCode}} response
   */
  updateQrCode_(response) {
    if (!response || !response.qrCode) {
      return;
    }
    this.canvasSize_ =
        response.qrCode.size * QR_CODE_TILE_SIZE + 2 * QR_CODE_PADDING;
    const context = this.getCanvasContext_();
    context.clearRect(0, 0, this.canvasSize_, this.canvasSize_);
    context.fillStyle = QR_CODE_FILL_STYLE;
    let index = 0;
    for (let x = 0; x < response.qrCode.size; x++) {
      for (let y = 0; y < response.qrCode.size; y++) {
        if (response.qrCode.data[index]) {
          context.fillRect(
              x * QR_CODE_TILE_SIZE + QR_CODE_PADDING,
              y * QR_CODE_TILE_SIZE + QR_CODE_PADDING, QR_CODE_TILE_SIZE,
              QR_CODE_TILE_SIZE);
        }
        index++;
      }
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  rsuCodeIsPlausible_() {
    return !!this.rsuCode_ && this.rsuCode_.length == 8;
  }

  /**
   * @protected
   * @param {!Event} event
   */
  onRsuCodeChanged_(event) {
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: !this.rsuCodeIsPlausible_()},
        ));
  }

  /**
   * @private
   * @return {!CanvasRenderingContext2D}
   */
  getCanvasContext_() {
    return this.shadowRoot.querySelector('#qrCodeCanvas').getContext('2d');
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.rsuCode_) {
      return this.shimlessRmaService_.setRsuDisableWriteProtectCode(
          this.rsuCode_);
    } else {
      return Promise.reject(new Error('No RSU code set'));
    }
  }

  /** @private */
  setRsuInstructionsText_() {
    this.rsuInstructionsText_ =
        this.i18nAdvanced('rsuCodeInstructionsText', {attrs: ['id']});
    const linkElement = this.shadowRoot.querySelector('#rsuCodeDialogLink');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', () => {
      if (this.allButtonsDisabled) {
        return;
      }

      this.shadowRoot.querySelector('#rsuChallengeDialog').showModal();
    });
  }

  /**
   * @return {string}
   * @private
   */
  computeRsuChallengeLinkText_() {
    const unlockPageUrl =
        'https://chromeos.google.com/partner/console/cr50reset?challenge=';
    return unlockPageUrl + this.rsuChallenge_ + '&hwid=' + this.rsuHwid_;
  }

  /** @private */
  closeDialog_() {
    this.shadowRoot.querySelector('#rsuChallengeDialog').close();
  }

  /**
   * @return {string}
   * @private
   */
  computeRsuCodeLengthLabel_() {
    return this.rsuCode_.length + '/8';
  }
}

customElements.define(
    OnboardingEnterRsuWpDisableCodePage.is,
    OnboardingEnterRsuWpDisableCodePage);
