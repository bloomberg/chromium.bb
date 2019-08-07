// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle keyboard input.
 */
class KeyEventHandler {
  /**
   * @param {!SwitchAccessInterface} switchAccess
   */
  constructor(switchAccess) {
    /**
     * Switch Access reference.
     * @private {!SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    /** @private {function(number)|undefined} */
    this.keycodeCallback_;

    this.init_();
  }

  /**
   * Listens for keycodes. When they're received, they are passed to |callback|.
   * @param {function(number)} callback
   */
  listenForKeycodes(callback) {
    this.keycodeCallback_ = callback;
    chrome.accessibilityPrivate.forwardKeyEventsToSwitchAccess(true);
  }

  /**
   * Stop listening for keycodes.
   */
  stopListeningForKeycodes() {
    this.keycodeCallback_ = undefined;
    chrome.accessibilityPrivate.forwardKeyEventsToSwitchAccess(false);
  }

  /**
   * Update the keyboard keys captured by Switch Access to those stored in
   * preferences.
   */
  updateSwitchAccessKeys() {
    let keyCodes = [];
    for (const command of this.switchAccess_.getCommands()) {
      const keyCode = this.keyCodeFor_(command);
      if (!keyCode)
        continue;
      if ((keyCode >= '0'.charCodeAt(0) && keyCode <= '9'.charCodeAt(0)) ||
          (keyCode >= 'A'.charCodeAt(0) && keyCode <= 'Z'.charCodeAt(0))) {
        keyCodes.push(keyCode);
      }
    }
    chrome.accessibilityPrivate.setSwitchAccessKeys(keyCodes);
  }

  /**
   * Forwards the current key code to the callback.
   * @param {!Event} event
   * @private
   */
  forwardKeyCode_(event) {
    if (this.keycodeCallback_)
      this.keycodeCallback_(event.keyCode);
  }

  /**
   * Run the command associated with the passed keyboard event.
   * @param {!Event} event
   * @private
   */
  handleSwitchActivated_(event) {
    for (const command of this.switchAccess_.getCommands()) {
      if (this.keyCodeFor_(command) === event.keyCode) {
        this.switchAccess_.runCommand(command);
        this.switchAccess_.performedUserAction();
        return;
      }
    }
  }

  /**
   * Set up key listener.
   * @private
   */
  init_() {
    this.updateSwitchAccessKeys();
    document.addEventListener('keyup', this.handleSwitchActivated_.bind(this));
    document.addEventListener('keydown', this.forwardKeyCode_.bind(this));
  }

  /**
   * Return the key code that |command| maps to.
   *
   * @param {SAConstants.Command} command
   * @return {number|null}
   */
  keyCodeFor_(command) {
    // All commands are preferences (see switch_access_constants.js).
    const preference = /** @type {SAConstants.Preference} */ (command);
    return this.switchAccess_.getNumberPreferenceIfDefined(preference);
  }
}
