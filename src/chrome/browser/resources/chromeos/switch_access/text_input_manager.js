// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle text input for improved accuracy and efficiency.
 */

const TextInputManager = {
  /** @type {number} */
  KEY_PRESS_DURATION: 100,

  /**
   * Sends a keyEvent to click the center of the provided node.
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean} Whether a key was pressed.
   */
  pressKey: (node) => {
    if (node.role !== chrome.automation.RoleType.BUTTON)
      return false;
    if (!TextInputManager.inVirtualKeyboard_(node))
      return false;

    const x = node.location.left + Math.round(node.location.width / 2);
    const y = node.location.top + Math.round(node.location.height / 2);

    chrome.accessibilityPrivate.sendSyntheticMouseEvent({
      type: chrome.accessibilityPrivate.SyntheticMouseEventType.PRESS,
      x: x,
      y: y
    });

    setTimeout(
        () => chrome.accessibilityPrivate.sendSyntheticMouseEvent({
          type: chrome.accessibilityPrivate.SyntheticMouseEventType.RELEASE,
          x: x,
          y: y
        }),
        TextInputManager.KEY_PRESS_DURATION);

    return true;
  },

  /**
   * Checks if |node| is in the virtual keyboard.
   * @private
   * @param {!chrome.automation.AutomationNode} node
   * @return {boolean}
   */
  inVirtualKeyboard_: (node) => {
    if (node.role === chrome.automation.RoleType.KEYBOARD)
      return true;
    if (node.parent)
      return TextInputManager.inVirtualKeyboard_(node.parent);
    return false;
  },
};
