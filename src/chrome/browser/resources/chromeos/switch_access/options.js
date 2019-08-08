// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage the options page.
 */
class SwitchAccessOptions {
  constructor() {
    /**
     * SwitchAccess reference.
     * @private {!SwitchAccessInterface}
     */
    this.switchAccess_ = chrome.extension.getBackgroundPage().switchAccess;

    this.init_();
  }

  /**
   * Initialize the options page by setting all elements representing a user
   * preference to show the correct value.
   *
   * @private
   */
  init_() {
    document.addEventListener('change', this.handleInputChange_.bind(this));
    chrome.storage.onChanged.addListener(this.handleStorageChange_.bind(this));

    document.getElementById('enableAutoScan').checked =
        this.switchAccess_.getBooleanPreference(
            SAConstants.Preference.ENABLE_AUTO_SCAN);
    document.getElementById('autoScanTime').value =
        this.switchAccess_.getNumberPreference(
            SAConstants.Preference.AUTO_SCAN_TIME) /
        1000;

    for (const command of this.switchAccess_.getCommands()) {
      // All commands are preferences (see switch_access_constants.js).
      const pref = /** @type {SAConstants.Preference} */ (command);
      const keyCode = this.switchAccess_.getNumberPreferenceIfDefined(pref);
      if (keyCode)
        document.getElementById(command).value = String.fromCharCode(keyCode);
    }
  }

  /**
   * Handle a change by the user to an element representing a user preference.
   *
   * @param {!Event} event
   * @private
   */
  handleInputChange_(event) {
    const input = event.target;
    switch (input.id) {
      case 'enableAutoScan':
        this.switchAccess_.setPreference(input.id, input.checked);
        break;
      case 'autoScanTime':
        const oldVal = this.switchAccess_.getNumberPreference(input.id);
        const val = Number(input.value) * 1000;
        const min = Number(input.min) * 1000;
        if (this.isValidScanTimeInput_(val, oldVal, min)) {
          input.value = Number(input.value);
          this.switchAccess_.setPreference(input.id, val);
        } else {
          input.value = oldVal;
        }
        break;
      default:
        if (this.switchAccess_.hasCommand(input.id)) {
          // If the input is empty, remove any existing key-mapping.
          if (!input.value) {
            chrome.storage.sync.remove(input.id);
            break;
          }
          const keyCode = input.value.toUpperCase().charCodeAt(0);
          if (this.isValidKeyCode_(keyCode)) {
            input.value = input.value.toUpperCase();
            this.switchAccess_.setPreference(input.id, keyCode);
          } else {
            const oldKeyCode =
                this.switchAccess_.getNumberPreferenceIfDefined(input.id);
            if (oldKeyCode)
              input.value = String.fromCharCode(oldKeyCode);
            else
              input.value = '';
          }
        }
    }
  }

  /**
   * Return true if |keyCode| is a letter or number, and if it is not already
   * being used.
   *
   * @param {number} keyCode
   * @return {boolean}
   */
  isValidKeyCode_(keyCode) {
    return ((keyCode >= '0'.charCodeAt(0) && keyCode <= '9'.charCodeAt(0)) ||
            (keyCode >= 'A'.charCodeAt(0) && keyCode <= 'Z'.charCodeAt(0))) &&
        !this.switchAccess_.keyCodeIsUsed(keyCode);
  }

  /**
   * Return true if the input is a valid autoScanTime input. Otherwise, return
   * false.
   *
   * @param {number} value
   * @param {number} oldValue
   * @param {number} min
   * @return {boolean}
   */
  isValidScanTimeInput_(value, oldValue, min) {
    return (value !== oldValue) && (value >= min);
  }

  /**
   * Handle a change in user preferences.
   *
   * @param {!Object} storageChanges
   * @param {string} areaName
   * @private
   */
  handleStorageChange_(storageChanges, areaName) {
    for (let key of Object.keys(storageChanges)) {
      const newValue = storageChanges[key].newValue;
      switch (key) {
        case 'enableAutoScan':
          document.getElementById(key).checked = newValue;
          break;
        case 'autoScanTime':
          document.getElementById(key).value = newValue / 1000;
          break;
        default:
          if (!this.switchAccess_.hasCommand(key))
            break;
          if (newValue)
            document.getElementById(key).value = String.fromCharCode(newValue);
          else
            document.getElementById(key).value = '';
      }
    }
  }
}

document.addEventListener('DOMContentLoaded', function() {
  new SwitchAccessOptions();
});
