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
   * Must be kept in sync with accessibility_manager.cc.
   * @type {string}
   * @const
   */
  PRIMARY_FOCUS: 'primary',

  /**
   * The ID used for the focus ring around the current scope.
   * Must be kept in sync with accessibility_manager.cc.
   * @type {string}
   * @const
   */
  SCOPE_FOCUS: 'scope',

  /**
   * Actions available in the Switch Access Menu.
   * @enum {string}
   * @const
   */
  MenuAction: {
    DECREMENT: chrome.automation.ActionType.DECREMENT,
    DICTATION: 'dictation',
    INCREMENT: chrome.automation.ActionType.INCREMENT,
    // This opens the Switch Access settings in a new Chrome tab.
    OPTIONS: 'options',
    SCROLL_BACKWARD: chrome.automation.ActionType.SCROLL_BACKWARD,
    SCROLL_DOWN: chrome.automation.ActionType.SCROLL_DOWN,
    SCROLL_FORWARD: chrome.automation.ActionType.SCROLL_FORWARD,
    SCROLL_LEFT: chrome.automation.ActionType.SCROLL_LEFT,
    SCROLL_RIGHT: chrome.automation.ActionType.SCROLL_RIGHT,
    SCROLL_UP: chrome.automation.ActionType.SCROLL_UP,
    // This either performs the default action or enters a new scope, as
    // applicable.
    SELECT: 'select',
    SHOW_CONTEXT_MENU: chrome.automation.ActionType.SHOW_CONTEXT_MENU
  },

  /**
   * Empty location, used for hiding the menu.
   * @const
   */
  EMPTY_LOCATION: {left: 0, top: 0, width: 0, height: 0}
};
