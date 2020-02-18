// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage user preferences.
 */
class SwitchAccessPreferences {
  /**
   * @param {!SwitchAccessInterface} switchAccess
   * @param {function()} onReady A callback that is called after the
   *     initial preferences are loaded.
   */
  constructor(switchAccess, onReady) {
    /**
     * SwitchAccess reference.
     * @private {!SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    /**
     * User preferences, initially set to the default preference values.
     * @private {!Map<SAConstants.Preference,
     *                chrome.settingsPrivate.PrefObject>}
     */
    this.preferences_ = new Map();

    this.init_(onReady);
  }

  /**
   * Updates the cached preferences.
   * @param {!Array<chrome.settingsPrivate.PrefObject>} preferences
   * @param {function()=} opt_onUpdate Optional callback, called after the
   *     cached preferences have been updated.
   * @private
   */
  updateFromSettings_(preferences, opt_onUpdate) {
    let updatedPreferences = {};
    for (const pref of preferences) {
      // Ignore preferences that are not used by Switch Access.
      if (!Object.values(SAConstants.Preference).includes(pref.key))
        continue;

      const key = /** @type {SAConstants.Preference} */ (pref.key);
      const oldPrefObject = this.preferences_.get(key);
      if (!oldPrefObject || oldPrefObject.value !== pref.value) {
        this.preferences_.set(key, pref);
        updatedPreferences[key] = pref.value;
      }
    }
    if (Object.keys(updatedPreferences).length > 0)
      this.switchAccess_.onPreferencesChanged(updatedPreferences);

    if (opt_onUpdate)
      opt_onUpdate();
  }

  /**
   * @param {function()} onReady Callback that is called once preference
   *     cache has been successfully initialized.
   * @private
   */
  init_(onReady) {
    chrome.settingsPrivate.onPrefsChanged.addListener(
        this.updateFromSettings_.bind(this));
    chrome.settingsPrivate.getAllPrefs(
        (prefs) => this.updateFromSettings_(prefs, onReady));
  }

  /**
   * Set the value of the preference |name| to |value| in |chrome.storage.sync|.
   * |this.preferences_| is not set until |handleStorageChange_|.
   *
   * @param {SAConstants.Preference} name
   * @param {boolean|number} value
   */
  setPreference(name, value) {
    chrome.settingsPrivate.setPref(name, value);
  }

  /**
   * Get the boolean value for the given name. Will throw a type error if the
   * value associated with |name| is not a boolean, or is undefined.
   *
   * @param  {SAConstants.Preference} name
   * @return {boolean}
   */
  getBooleanPreference(name) {
    const pref = this.preferences_.get(name);
    if (pref && pref.type === chrome.settingsPrivate.PrefType.BOOLEAN)
      return /** @type {boolean} */ (pref.value);
    else
      throw new TypeError('No value of boolean type named \'' + name + '\'');
  }

  /**
   * Get the string value for the given name. Will throw a type error if the
   * value associated with |name| is not a string, or is undefined.
   *
   * @param {SAConstants.Preference} name
   * @return {string}
   */
  getStringPreference(name) {
    const pref = this.preferences_.get(name);
    if (pref && pref.type === chrome.settingsPrivate.PrefType.STRING)
      return /** @type {string} */ (pref.value);
    else
      throw new TypeError('No value of string type named \'' + name + '\'');
  }

  /**
   * Get the number value for the given name. Will throw a type error if the
   * value associated with |name| is not a number, or is undefined.
   *
   * @param  {SAConstants.Preference} name
   * @return {number}
   */
  getNumberPreference(name) {
    const value = this.getNumberPreferenceIfDefined(name);
    if (!value)
      throw new TypeError('No value of number type named \'' + name + '\'');
    return value;
  }

  /**
   * Get the number value for the given name, or |null| if the value is not a
   * number or does not exist.
   *
   * @param {SAConstants.Preference} name
   * @return {number|null}
   */
  getNumberPreferenceIfDefined(name) {
    const pref = this.preferences_.get(name);
    if (pref && pref.type === chrome.settingsPrivate.PrefType.NUMBER)
      return /** @type {number} */ (pref.value);
    return null;
  }
}

