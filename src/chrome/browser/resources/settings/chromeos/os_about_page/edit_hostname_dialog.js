// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {number} */
const MAX_INPUT_LENGTH = 15;

/** @type {number} */
const MIN_INPUT_LENGTH = 1;

const UNALLOWED_CHARACTERS = '[^0-9A-Za-z-]+';

/** @type {RegExp} */
const EMOJI_REGEX_EXP =
    /(\u00a9|\u00ae|[\u2000-\u3300]|\ud83c[\ud000-\udfff]|\ud83d[\ud000-\udfff]|\ud83e[\ud000-\udfff])/gi;

/**
 * @fileoverview 'edit-hostname-dialog' is a component allowing the
 * user to edit the device hostname.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import '//resources/polymer/v3_0/iron-selector/iron-selector.js';
import '../../settings_shared_css.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';

import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, AboutPageUpdateInfo, BrowserChannel, browserChannelToI18nId, ChannelInfo, isTargetChannelMoreStable, RegulatoryInfo, TPMFirmwareUpdateStatusChangedEvent, UpdateStatus, UpdateStatusChangedEvent, VersionInfo} from './about_page_browser_proxy.js';

import {DeviceNameBrowserProxy, DeviceNameBrowserProxyImpl} from './device_name_browser_proxy.js';
import {SetDeviceNameResult} from './device_name_util.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'edit-hostname-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @private {string} */
    deviceName_: {
      type: String,
      value: '',
      observer: 'onDeviceNameChanged_',
    },

    /** @private {boolean} */
    isInputInvalid_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @private {string} */
    inputCountString_: {
      type: String,
      computed: 'computeInputCountString_(deviceName_)',
    }
  },

  /**
   * Returns a formatted string containing the current number of characters
   * entered in the input compared to the maximum number of characters allowed.
   * @return {string}
   * @private
   */
  computeInputCountString_() {
    return this.i18n(
        'aboutDeviceNameInputCharacterCount',
        this.deviceName_.length.toLocaleString(),
        MAX_INPUT_LENGTH.toLocaleString());
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  },

  /**
   * Observer for deviceName_ that sanitizes its value by removing any
   * Emojis and truncating it to MAX_INPUT_LENGTH. This method will be
   * recursively called until deviceName_ is fully sanitized.
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onDeviceNameChanged_(newValue, oldValue) {
    if (oldValue) {
      const sanitizedOldValue = oldValue.replace(EMOJI_REGEX_EXP, '');
      // If sanitizedOldValue.length > MAX_INPUT_LENGTH, the user attempted to
      // enter more than the max limit, this method was called and it was
      // truncated, and then this method was called one more time.
      this.isInputInvalid_ = sanitizedOldValue.length > MAX_INPUT_LENGTH;
    } else {
      this.isInputInvalid_ = false;
    }

    // Remove all Emojis from the name.
    const sanitizedDeviceName = this.deviceName_.replace(EMOJI_REGEX_EXP, '');

    // Truncate the name to MAX_INPUT_LENGTH.
    this.deviceName_ = sanitizedDeviceName.substring(0, MAX_INPUT_LENGTH);

    if (this.deviceName_.length < MIN_INPUT_LENGTH ||
        this.deviceName_.match(UNALLOWED_CHARACTERS)) {
      this.isInputInvalid_ = true;
    }
  },

  /** @private */
  onDoneTap_() {
    DeviceNameBrowserProxyImpl.getInstance()
        .attemptSetDeviceName(this.deviceName_)
        .then(result => {
          this.handleSetDeviceNameResponse_(result);
        });
    this.$.dialog.close();
  },

  /**
   * @param {SetDeviceNameResult} result
   * @private
   */
  handleSetDeviceNameResponse_(result) {
    if (result !== SetDeviceNameResult.UPDATE_SUCCESSFUL) {
      console.error('ERROR IN UPDATE', result);
    }
  },
});
