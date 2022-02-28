// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './firmware_shared_css.js';
import './firmware_shared_fonts.js';
import './strings.m.js';

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FirmwareUpdate, InstallationProgress, UpdateControllerInterface} from './firmware_update_types.js';
import {getUpdateController} from './mojo_interface_provider.js';

/** @enum {number} */
export const DialogState = {
  CLOSED: 0,
  DEVICE_PREP: 1,
  UPDATING: 2,
  UPDATE_DONE: 3,
};

/**
 * @fileoverview
 * 'firmware-update-dialog' displays information related to a firmware update.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const FirmwareUpdateDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class FirmwareUpdateDialogElement extends
    FirmwareUpdateDialogElementBase {
  static get is() {
    return 'firmware-update-dialog';
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

      /** @type {!DialogState} */
      dialogState: {
        type: Number,
        value: DialogState.CLOSED,
      },

      /** @type {?InstallationProgress} */
      installationProgress: {
        type: Object,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!UpdateControllerInterface} */
    this.updateController_ = getUpdateController();

    /**
     * Event callback for 'open-device-prep-dialog'.
     * @param {!Event} e
     * @private
     */
    this.openDevicePrepDialog_ = (e) => {
      this.update = e.detail.update;
      this.dialogState = DialogState.DEVICE_PREP;
    };

    /**
     * Event callback for 'open-update-dialog'.
     * @param {!Event} e
     * @private
     */
    this.openUpdateDialog_ = (e) => {
      this.update = e.detail.update;
      this.startUpdate_();
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    window.addEventListener(
        'open-device-prep-dialog', (e) => this.openDevicePrepDialog_(e));

    window.addEventListener(
        'open-update-dialog', (e) => this.openUpdateDialog_(e));
  }

  /**
   * Implements UpdateProgressObserver.onProgressChanged
   * @param {!InstallationProgress} installationProgress
   */
  onProgressChanged(installationProgress) {
    this.installationProgress = installationProgress;
    if (installationProgress.percentage === 100) {
      this.dialogState = DialogState.UPDATE_DONE;
    }
  }

  /**
   * @protected
   * @return {boolean}
   */
  shouldShowDevicePrepDialog_() {
    return this.dialogState === DialogState.DEVICE_PREP;
  }

  /** @protected */
  closeDialog_() {
    this.dialogState = DialogState.CLOSED;
    this.installationProgress = null;
  }

  /** @protected */
  startUpdate_() {
    this.dialogState = DialogState.UPDATING;
    this.updateController_.startUpdate(this.update.deviceId, this);
  }

  /**
   * @protected
   * @return {boolean}
   */
  shouldShowUpdateDialog_() {
    return this.isUpdateInProgress_() ||
        this.dialogState === DialogState.UPDATE_DONE;
  }

  /**
   * @protected
   * @return {number}
   */
  computePercentageValue_() {
    if (this.installationProgress && this.installationProgress.percentage) {
      return this.installationProgress.percentage;
    }
    return 0;
  }

  /**
   * @protected
   * @return {boolean}
   */
  isUpdateInProgress_() {
    return this.dialogState === DialogState.UPDATING;
  }

  /**
   * @protected
   * @return {string}
   */
  computeUpdateDialogTitle_() {
    return this.isUpdateInProgress_() ?
        this.i18n('updating', this.update.deviceName) :
        this.i18n('deviceUpToDate', this.update.deviceName);
  }

  /**
   * @protected
   * @return {string}
   */
  computeProgressText_() {
    return this.i18n('installing', this.computePercentageValue_());
  }

  /**
   * @protected
   * @return {string}
   */
  computeUpdateDialogBodyText_() {
    const {deviceName, version} = this.update;
    return this.dialogState === DialogState.UPDATE_DONE ?
        this.i18n('hasBeenUpdated', deviceName, version) :
        this.i18n('updatingInfo');
  }
}

customElements.define(
    FirmwareUpdateDialogElement.is, FirmwareUpdateDialogElement);
