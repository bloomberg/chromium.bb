// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage user preferences.
 */
class SwitchAccessPreferences {
  /** @private */
  constructor() {
    /**
     * User preferences, initially set to the default preference values.
     * @private {!Map<SAConstants.Preference,
     *                chrome.settingsPrivate.PrefObject>}
     */
    this.preferences_ = new Map();

    this.init_();
  }

  // =============== Static Methods ==============

  static initialize() {
    SwitchAccessPreferences.instance = new SwitchAccessPreferences();
  }

  // =============== Private Methods ==============

  /**
   * Get the boolean value for the given name, or |null| if the value is not a
   * boolean or does not exist.
   *
   * @param  {SAConstants.Preference} name
   * @return {boolean|null}
   * @private
   */
  getBoolean_(name) {
    const pref = this.preferences_.get(name);
    if (pref && pref.type === chrome.settingsPrivate.PrefType.BOOLEAN) {
      return /** @type {boolean} */ (pref.value);
    } else {
      return null;
    }
  }

  /**
   * Get the number value for the given name, or |null| if the value is not a
   * number or does not exist.
   *
   * @param {SAConstants.Preference} name
   * @return {number|null}
   * @private
   */
  getNumber_(name) {
    const pref = this.preferences_.get(name);
    if (pref && pref.type === chrome.settingsPrivate.PrefType.NUMBER) {
      return /** @type {number} */ (pref.value);
    }
    return null;
  }

  /** @private */
  init_() {
    chrome.settingsPrivate.onPrefsChanged.addListener(
        this.updateFromSettings_.bind(this));
    chrome.settingsPrivate.getAllPrefs(
        (prefs) => this.updateFromSettings_(prefs, true /* isFirstLoad */));
  }

  /**
   * Whether the current settings configuration is reasonably usable;
   * specifically, whether there is a way to select and a way to navigate.
   * @return {boolean}
   * @private
   */
  settingsAreConfigured_() {
    const selectSetting =
        this.getNumber_(SAConstants.Preference.SELECT_SETTING);
    const nextSetting = this.getNumber_(SAConstants.Preference.NEXT_SETTING);
    const previousSetting =
        this.getNumber_(SAConstants.Preference.PREVIOUS_SETTING);
    const autoScanEnabled =
        !!this.getBoolean_(SAConstants.Preference.AUTO_SCAN_ENABLED);

    if (!selectSetting) {
      return false;
    }

    if (nextSetting || previousSetting) {
      return true;
    }

    return autoScanEnabled;
  }

  /**
   * Updates the cached preferences.
   * @param {!Array<chrome.settingsPrivate.PrefObject>} preferences
   * @param {boolean} isFirstLoad
   * @private
   */
  updateFromSettings_(preferences, isFirstLoad = false) {
    for (const pref of preferences) {
      // Ignore preferences that are not used by Switch Access.
      if (!this.usesPreference_(pref)) {
        continue;
      }

      const key = /** @type {SAConstants.Preference} */ (pref.key);
      const oldPrefObject = this.preferences_.get(key);
      if (!oldPrefObject || oldPrefObject.value !== pref.value) {
        this.preferences_.set(key, pref);
        switch (key) {
          case SAConstants.Preference.AUTO_SCAN_ENABLED:
            if (pref.type === chrome.settingsPrivate.PrefType.BOOLEAN) {
              AutoScanManager.setEnabled(/** @type {boolean} */ (pref.value));
            }
            break;
          case SAConstants.Preference.AUTO_SCAN_TIME:
            if (pref.type === chrome.settingsPrivate.PrefType.NUMBER) {
              AutoScanManager.setPrimaryScanTime(
                  /** @type {number} */ (pref.value));
            }
            break;
          case SAConstants.Preference.AUTO_SCAN_KEYBOARD_TIME:
            if (pref.type === chrome.settingsPrivate.PrefType.NUMBER) {
              AutoScanManager.setKeyboardScanTime(
                  /** @type {number} */ (pref.value));
            }
            break;
        }
      }
    }

    if (isFirstLoad && !this.settingsAreConfigured_()) {
      chrome.accessibilityPrivate.openSettingsSubpage(
          'manageAccessibility/switchAccess');
    }
  }

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   */
  usesPreference_(pref) {
    return Object.values(SAConstants.Preference).includes(pref.key);
  }
}
