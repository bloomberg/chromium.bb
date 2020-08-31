// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles gesture-based commands.
 */

goog.provide('GestureCommandHandler');

goog.require('CommandHandler');
goog.require('EventSourceState');
goog.require('GestureCommandData');

goog.scope(function() {
const RoleType = chrome.automation.RoleType;

/**
 * Global setting for the enabled state of this handler.
 * @param {boolean} state
 */
GestureCommandHandler.setEnabled = function(state) {
  GestureCommandHandler.enabled_ = state;
};

/**
 * Global setting for the enabled state of this handler.
 * @return {boolean}
 */
GestureCommandHandler.getEnabled = function() {
  return GestureCommandHandler.enabled_;
};

/**
 * Handles accessibility gestures from the touch screen.
 * @param {string} gesture The gesture to handle, based on the
 *     ax::mojom::Gesture enum defined in ui/accessibility/ax_enums.mojom
 * @private
 */
GestureCommandHandler.onAccessibilityGesture_ = function(gesture) {
  if (!GestureCommandHandler.enabled_) {
    return;
  }

  EventSourceState.set(EventSourceType.TOUCH_GESTURE);

  const commandData = GestureCommandData.GESTURE_COMMAND_MAP[gesture];
  if (!commandData) {
    return;
  }

  Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);

  // Map gestures to arrow keys while within menus belonging to the desktop or
  // generally in the ChromeVox Panel.
  if (ChromeVoxState.instance.currentRange) {
    const range = ChromeVoxState.instance.currentRange;
    if (commandData.menuKeyOverride && range.start && range.start.node &&
        ((range.start.node.role == RoleType.MENU_ITEM &&
          range.start.node.root.role == RoleType.DESKTOP) ||
         range.start.node.root.docUrl.indexOf(
             chrome.extension.getURL('chromevox/panel/panel.html')) == 0)) {
      const key = commandData.menuKeyOverride;
      BackgroundKeyboardHandler.sendKeyPress(key.keyCode, key.modifiers);
      return;
    }
  }

  if (!ChromeVoxState.instance.currentRange && commandData.shouldRecoverRange) {
    const recoverTo = DesktopAutomationHandler.instance.lastHoverTarget;
    if (recoverTo) {
      ChromeVoxState.instance.setCurrentRange(
          cursors.Range.fromNode(recoverTo));
    }
  }

  const command = commandData.command;
  if (command) {
    CommandHandler.onCommand(command);
  }
};

/** @private {boolean} */
GestureCommandHandler.enabled_ = true;

/** Performs global setup. @private */
GestureCommandHandler.init_ = function() {
  chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
      GestureCommandHandler.onAccessibilityGesture_);
};

/**
 * The global granularity for gestures.
 * @type {GestureGranularity}
 */
GestureCommandHandler.granularity = GestureGranularity.LINE;

GestureCommandHandler.init_();
});  // goog.scope
