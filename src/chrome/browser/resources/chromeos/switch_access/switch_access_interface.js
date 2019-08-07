// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface for controllers to interact with main SwitchAccess class.
 * @interface
 */
class SwitchAccessInterface {
  /**
   * Open and jump to the Switch Access menu.
   */
  enterMenu() {}

  /**
   * Move to the next interesting node.
   */
  moveForward() {}

  /**
   * Move to the previous interesting node.
   */
  moveBackward() {}

  /**
   * Perform the default action on the current node.
   */
  selectCurrentNode() {}

  /**
   * Open the options page in a new tab.
   */
  showOptionsPage() {}

  /**
   * Return a list of the names of all user commands.
   * @return {!Array<!SAConstants.Command>}
   */
  getCommands() {}

  /**
   * Checks if the given string is a valid Switch Access command.
   * @param {string} command
   * @return {boolean}
   */
  hasCommand(command) {}

  /**
   * Return the default key code for a command.
   *
   * @param {!SAConstants.Command} command
   * @return {number}
   */
  getDefaultKeyCodeFor(command) {}

  /**
   * Forwards keycodes received from keyPress events to |callback|.
   * @param {function(number)} callback
   */
  listenForKeycodes(callback) {}

  /**
   * Stops forwarding keycodes.
   */
  stopListeningForKeycodes() {}

  /**
   * Run the function binding for the specified command.
   * @param {!SAConstants.Command} command
   */
  runCommand(command) {}

  /**
   * Perform actions as the result of actions by the user. Currently, restarts
   * auto-scan if it is enabled.
   */
  performedUserAction() {}

  /**
   * Handle a change in user preferences.
   * @param {!Object} changes
   */
  onPreferencesChanged(changes) {}

  /**
   * Set the value of the preference |key| to |value| in chrome.storage.sync.
   * The behavior is not updated until the storage update is complete.
   *
   * @param {string} key
   * @param {boolean|string|number} value
   */
  setPreference(key, value) {}

  /**
   * Get the value of type 'boolean' of the preference |key|. Will throw a type
   * error if the value of |key| is not 'boolean'.
   *
   * @param  {string} key
   * @return {boolean}
   */
  getBooleanPreference(key) {}

  /**
   * Get the value of type 'number' of the preference |key|. Will throw a type
   * error if the value of |key| is not 'number'.
   *
   * @param  {string} key
   * @return {number}
   */
  getNumberPreference(key) {}

  /**
   * Returns true if |keyCode| is already used to run a command from the
   * keyboard.
   *
   * @param {number} keyCode
   * @return {boolean}
   */
  keyCodeIsUsed(keyCode) {}

  /**
   * Sets up the connection between the menuPanel and the menuManager.
   * @param {!PanelInterface} menuPanel
   * @return {MenuManager}
   */
  connectMenuPanel(menuPanel) {}
}
