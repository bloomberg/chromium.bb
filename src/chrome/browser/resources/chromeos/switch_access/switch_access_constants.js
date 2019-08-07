// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Constants used in Switch Access */
const SAConstants = {
  /**
   * The ID of the menu panel.
   * This must be kept in sync with the div ID in menu_panel.html.
   * @type {string}
   * @const
   */
  MENU_ID: 'switchaccess_menu_actions',

  /**
   * The ID of the back button.
   * This must be kept in sync with the back button ID in menu_panel.html.
   * @type {string}
   * @const
   */
  BACK_ID: 'back',

  /**
   * The name of the focus class for the menu.
   * This must be kept in sync with the class name in menu_panel.css.
   * @type {string}
   * @const
   */
  FOCUS_CLASS: 'focus',

  /**
   * The ID used for the focus ring around the current element.
   * @type {string}
   * @const
   */
  PRIMARY_FOCUS_ID: 'primary',

  /**
   * The ID used for the focus ring around the current scope.
   * @type {string}
   * @const
   */
  SCOPE_FOCUS_ID: 'scope',

  /**
   * The ID used for the focus ring around the active text input.
   * @type {string}
   * @const
   */
  TEXT_FOCUS_ID: 'text',

  /**
   * The inner color of the focus rings.
   * @type {string}
   * @const
   */
  PRIMARY_FOCUS_COLOR: '#8ab4f8b8',

  /**
   * The outer color of the focus rings.
   * @type {string}
   * @const
   */
  SECONDARY_FOCUS_COLOR: '#0003',

  /**
   * The amount of space (in px) needed to fit a focus ring around an element.
   * @type {number}
   * @const
   */
  FOCUS_RING_BUFFER: 4,

  /**
   * The delay between keydown and keyup events on the virtual keyboard,
   * allowing the key press animation to display.
   * @type {number}
   * @const
   */
  KEY_PRESS_DURATION_MS: 100,

  /**
   * Commands that can be assigned to specific switches.
   * @enum {string}
   * @const
   */
  Command: {MENU: 'menu', NEXT: 'next', PREVIOUS: 'previous', SELECT: 'select'},

  /**
   * Actions available in the Switch Access Menu.
   * @enum {string}
   * @const
   */
  MenuAction: {
    // Decrement the value of an input field.
    DECREMENT: chrome.automation.ActionType.DECREMENT,
    // Activate dictation for voice input to an editable text region.
    DICTATION: 'dictation',
    // Increment the value of an input field.
    INCREMENT: chrome.automation.ActionType.INCREMENT,
    // Open and jump to the virtual keyboard
    KEYBOARD: 'keyboard',
    // Open and jump to the Switch Access settings in a new Chrome tab.
    OPTIONS: 'options',
    // Scroll the current element (or its ancestor) logically backwards.
    // Primarily used by ARC++ apps.
    SCROLL_BACKWARD: chrome.automation.ActionType.SCROLL_BACKWARD,
    // Scroll the current element (or its ancestor) down.
    SCROLL_DOWN: chrome.automation.ActionType.SCROLL_DOWN,
    // Scroll the current element (or is ancestor) logically forwards.
    // Primarily used by ARC++ apps.
    SCROLL_FORWARD: chrome.automation.ActionType.SCROLL_FORWARD,
    // Scroll the current element (or its ancestor) left.
    SCROLL_LEFT: chrome.automation.ActionType.SCROLL_LEFT,
    // Scroll the current element (or its ancestor) right.
    SCROLL_RIGHT: chrome.automation.ActionType.SCROLL_RIGHT,
    // Scroll the current element (or its ancestor) up.
    SCROLL_UP: chrome.automation.ActionType.SCROLL_UP,
    // Either perform the default action or enter a new scope, as applicable.
    SELECT: 'select',
    // Show the system context menu for the current element.
    SHOW_CONTEXT_MENU: chrome.automation.ActionType.SHOW_CONTEXT_MENU
  },

  /**
   * Empty location, used for hiding the menu.
   * @const
   */
  EMPTY_LOCATION: {left: 0, top: 0, width: 0, height: 0}
};
