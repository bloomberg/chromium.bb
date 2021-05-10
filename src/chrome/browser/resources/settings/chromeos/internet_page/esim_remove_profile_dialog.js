// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to remove eSIM profile
 */

Polymer({
  is: 'esim-remove-profile-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @type {string} */
    iccid: {
      type: String,
      value: '',
    },

    /** @type {boolean} */
    showCellularDisconnectWarning: {
      type: Boolean,
      value: false,
    },

    /** @type {string} */
    esimProfileName_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    errorMessage_: {
      type: String,
      value: '',
    },

    /** @private {boolean} */
    isRemoveInProgress_: {
      type: Boolean,
      value: false,
    }
  },

  /** @private {?chromeos.cellularSetup.mojom.ESimProfileRemote} */
  esimProfileRemote_: null,

  /** @override */
  attached() {
    this.init_();
  },

  /** @private */
  async init_() {
    this.esimProfileRemote_ = await cellular_setup.getESimProfile(this.iccid);
    const profileProperties = await this.esimProfileRemote_.getProperties();
    this.esimProfileName_ = profileProperties.properties.nickname ?
        this.convertString16ToJSString_(profileProperties.properties.nickname) :
        this.convertString16ToJSString_(profileProperties.properties.name);
  },

  /**
   * Converts a mojoBase.mojom.String16 to a JavaScript String.
   * @param {?mojoBase.mojom.String16} str
   * @return {string}
   * @private
   */
  convertString16ToJSString_(str) {
    return str.data.map(ch => String.fromCodePoint(ch)).join('');
  },

  /**
   * @returns {string}
   * @private
   */
  getTitleString_() {
    return this.i18n('esimRemoveProfileDialogTitle', this.esimProfileName_);
  },

  /**
   * @param {Event} event
   * @private
   */
  onRemoveProfileTap_(event) {
    this.isRemoveInProgress_ = true;
    this.esimProfileRemote_.uninstallProfile().then(response => {
      this.handleRemoveProfileResponse(response.result);
    });
  },

  /**
   * @param {chromeos.cellularSetup.mojom.ESimOperationResult} result
   * @private
   */
  handleRemoveProfileResponse(result) {
    this.isRemoveInProgress_ = false;
    if (result === chromeos.cellularSetup.mojom.ESimOperationResult.kFailure) {
      this.errorMessage_ = this.i18n('eSimRemoveProfileDialogError');
      return;
    }
    this.$.dialog.close();
  },

  /**
   * @param {Event} event
   * @private
   */
  onCancelTap_(event) {
    this.$.dialog.close();
  }
});
