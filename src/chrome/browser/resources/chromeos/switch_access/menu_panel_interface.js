// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface for controllers to interact with main class of the Switch
 * Access menu panel.
 * @interface
 */
class PanelInterface {
  /**
   * Initialize the panel and buttons.
   */
  init() {}

  /**
   * Temporary function, until multiple focus rings is implemented.
   * Puts a focus ring around the given menu item.
   *
   * @param {string} id
   * @param {boolean} enable
   */
  setFocusRing(id, enable) {}

  /**
   * Sets which buttons are enabled/disabled, based on |actions|.
   * @param {!Array<string>} actions
   */
  setActions(actions) {}

  /**
   * Tells the menu panel to try to connect to the background page.
   */
  connectToBackground() {}
}
