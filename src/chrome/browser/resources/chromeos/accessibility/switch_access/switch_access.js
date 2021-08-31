// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Commands} from './commands.js';
import {Navigator} from './navigator.js';
import {KeyboardRootNode} from './nodes/keyboard_node.js';
import {PreferenceManager} from './preference_manager.js';
import {SAConstants} from './switch_access_constants.js';

const AutomationNode = chrome.automation.AutomationNode;

/**
 * The top-level class for the Switch Access accessibility feature. Handles
 * initialization and small matters that don't fit anywhere else in the
 * codebase.
 */
export class SwitchAccess {
  static initialize() {
    SwitchAccess.instance = new SwitchAccess();

    chrome.automation.getDesktop((desktop) => {
      // Navigator must be initialized first.
      Navigator.initializeSingletonInstance(desktop);

      Commands.initialize();
      KeyboardRootNode.startWatchingVisibility();
      PreferenceManager.initialize();
    });
  }

  /** @private */
  constructor() {
    /**
     * Feature flag controlling improvement of text input capabilities.
     * @private {boolean}
     */
    this.enableImprovedTextInput_ = false;

    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-switch-access-text', (result) => {
          this.enableImprovedTextInput_ = result;
        });

    /* @private {!SAConstants.Mode} */
    this.mode_ = SAConstants.Mode.ITEM_SCAN;
  }

  /**
   * Returns whether or not the feature flag
   * for improved text input is enabled.
   * @return {boolean}
   */
  improvedTextInputEnabled() {
    return this.enableImprovedTextInput_;
  }

  /** @return {!SAConstants.Mode} */
  get mode() {
    return this.mode_;
  }

  /** @param {!SAConstants.Mode} newMode */
  set mode(newMode) {
    this.mode_ = newMode;
  }

  /**
   * Helper function to robustly find a node fitting a given FindParams, even if
   * that node has not yet been created.
   * Used to find the menu and back button.
   * @param {!chrome.automation.FindParams} findParams
   * @param {!function(!AutomationNode): void} foundCallback
   */
  static findNodeMatching(findParams, foundCallback) {
    const desktop = Navigator.byItem.desktopNode;
    // First, check if the node is currently in the tree.
    let node = desktop.find(findParams);
    if (node) {
      foundCallback(node);
      return;
    }
    // If it's not currently in the tree, listen for changes to the desktop
    // tree.
    const eventHandler = new EventHandler(
        desktop, chrome.automation.EventType.CHILDREN_CHANGED,
        null /** callback */);

    const onEvent = (event) => {
      if (event.target.matches(findParams)) {
        // If the event target is the node we're looking for, we've found it.
        eventHandler.stop();
        foundCallback(event.target);
      } else if (event.target.children.length > 0) {
        // Otherwise, see if one of its children is the node we're looking for.
        node = event.target.find(findParams);
        if (node) {
          eventHandler.stop();
          foundCallback(node);
        }
      }
    };

    eventHandler.setCallback(onEvent);
    eventHandler.start();
  }

  /**
   * Creates and records the specified error.
   * @param {SAConstants.ErrorType} errorType
   * @param {string} errorString
   * @param {boolean} shouldRecover
   * @return {!Error}
   */
  static error(errorType, errorString, shouldRecover = false) {
    if (shouldRecover) {
      setTimeout(Navigator.byItem.moveToValidNode.bind(Navigator.byItem), 0);
    }
    const errorTypeCountForUMA = Object.keys(SAConstants.ErrorType).length;
    chrome.metricsPrivate.recordEnumerationValue(
        'Accessibility.CrosSwitchAccess.Error',
        /** @type {number} */ (errorType), errorTypeCountForUMA);
    return new Error(errorString);
  }
}
