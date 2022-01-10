// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComponentTypeToId} from './data.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CalibrationComponentStatus, CalibrationObserverInterface, CalibrationObserverReceiver, CalibrationOverallStatus, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'reimaging-calibration-page' is for recalibration of the
 * various components during the reimaging process.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ReimagingCalibrationRunPageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ReimagingCalibrationRunPage extends
    ReimagingCalibrationRunPageBase {
  static get is() {
    return 'reimaging-calibration-run-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @protected
       */
      calibrationComplete_: {
        type: Boolean,
        value: false,
      },

      /**
       * @protected
       */
      calibrationStatusMessage_: {
        type: String,
        value: '',
      }
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /** @private {!CalibrationObserverReceiver} */
    this.calibrationObserverReceiver_ = new CalibrationObserverReceiver(
        /** @type {!CalibrationObserverInterface} */ (this));

    this.shimlessRmaService_.observeCalibrationProgress(
        this.calibrationObserverReceiver_.$.bindNewPipeAndPassRemote());

    this.calibrationStatusMessage_ = this.i18n('runCalibrationStartingText');
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.calibrationComplete_) {
      return this.shimlessRmaService_.calibrationComplete();
    }
    return Promise.reject(new Error('Calibration is not complete.'));
  }

  /**
   * Implements CalibrationObserver.onCalibrationUpdated()
   * @param {!CalibrationComponentStatus} componentStatus
   */
  onCalibrationUpdated(componentStatus) {
    this.calibrationStatusMessage_ =
        this.getCalibrationStatusString_(componentStatus);
  }

  /**
   * Implements CalibrationObserver.onCalibrationUpdated()
   * @param {!CalibrationOverallStatus} status
   */
  onCalibrationStepComplete(status) {
    switch (status) {
      case CalibrationOverallStatus.kCalibrationOverallComplete:
        this.calibrationStatusMessage_ =
            this.i18n('runCalibrationCompleteText');
        this.calibrationComplete_ = true;
        this.dispatchEvent(new CustomEvent(
            'disable-next-button',
            {bubbles: true, composed: true, detail: false},
            ));
        break;
      case CalibrationOverallStatus.kCalibrationOverallCurrentRoundComplete:
      case CalibrationOverallStatus.kCalibrationOverallCurrentRoundFailed:
      case CalibrationOverallStatus.kCalibrationOverallInitializationFailed:
        this.dispatchEvent(new CustomEvent(
            'transition-state',
            {
              bubbles: true,
              composed: true,
              detail: (() => {
                return this.shimlessRmaService_.continueCalibration();
              })
            },
            ));
        break;
    }
  }

  /**
   * @param {!CalibrationComponentStatus} status
   * @return {string}
   * @private
   */
  getCalibrationStatusString_(status) {
    const componentType = this.i18n(ComponentTypeToId[status.component]);
    return this.i18n('runCalibrationCalibratingComponent', componentType);
  }
}

customElements.define(
    ReimagingCalibrationRunPage.is, ReimagingCalibrationRunPage);
