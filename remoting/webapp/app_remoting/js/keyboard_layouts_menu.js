// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class managing the host's available keyboard layouts, allowing the user to
 * select one that matches the local layout, or auto-selecting based on the
 * current locale.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {remoting.ContextMenuAdapter} adapter
 * @constructor
 */
remoting.KeyboardLayoutsMenu = function(adapter) {
  /** @private {remoting.ContextMenuAdapter} */
  this.adapter_ = adapter;
  /** @private {remoting.SubmenuManager} */
  this.submenuManager_ = new remoting.SubmenuManager(
      adapter,
      chrome.i18n.getMessage(/*i18n-content*/'KEYBOARD_LAYOUTS_SUBMENU_TITLE'),
      true);
  /** @private {string} */
  this.currentLayout_ = '';

  adapter.addListener(this.onContextMenu_.bind(this));
};

/**
 * @param {Array<string>} layouts The keyboard layouts available on the host,
 *   for example en-US, de-DE
 * @param {string} currentLayout The layout currently active on the host.
 */
remoting.KeyboardLayoutsMenu.prototype.setLayouts =
    function(layouts, currentLayout) {
  this.submenuManager_.removeAll();
  this.currentLayout_ = '';
  for (var i = 0; i < layouts.length; ++i) {
    this.submenuManager_.add(this.makeMenuId_(layouts[i]), layouts[i]);
  }
  // Pick a suitable default layout.
  this.getBestLayout_(layouts, currentLayout,
                      this.setLayout_.bind(this, false));
};

/**
 * Notify the host that a new keyboard layout has been selected.
 *
 * @param {boolean} saveToLocalStorage If true, save the specified layout to
 *     local storage.
 * @param {string} layout The new keyboard layout.
 * @private
 */
remoting.KeyboardLayoutsMenu.prototype.setLayout_ =
    function(saveToLocalStorage, layout) {
  if (this.currentLayout_ != '') {
    this.adapter_.updateCheckState(
        this.makeMenuId_(this.currentLayout_), false);
  }
  this.adapter_.updateCheckState(this.makeMenuId_(layout), true);
  this.currentLayout_ = layout;

  console.log("Setting the keyboard layout to '" + layout + "'");
  remoting.clientSession.sendClientMessage(
      'setKeyboardLayout',
      JSON.stringify({layout: layout}));
  if (saveToLocalStorage) {
    var params = {};
    params[remoting.KeyboardLayoutsMenu.KEY_] = layout;
    chrome.storage.local.set(params);
  }
};

/**
 * Choose the best keyboard from the alternatives, based on the following
 * algorithm:
 *   - Search local storage by for a preferred keyboard layout for the app;
 *     if it is found, prefer it over the current locale, falling back on the
 *     latter only if no match is found.
 *   - If the candidate layout matches one of the supported layouts, use it.
 *   - Otherwise, if the language portion of the candidate matches that of
 *     any of the supported layouts, use the first such layout (e.g, en-AU
 *     will match either en-US or en-GB, whichever appears first).
 *   - Otherwise, use the host's current layout.
 *
 * @param {Array<string>} layouts
 * @param {string} currentHostLayout
 * @param {function(string):void} onDone
 * @private
 */
remoting.KeyboardLayoutsMenu.prototype.getBestLayout_ =
    function(layouts, currentHostLayout, onDone) {
  /**
   * Extract the language id from a string that is either "language" (e.g.
   * "de") or "language-region" (e.g. "en-US").
   *
   * @param {string} layout
   * @return {string}
   */
  var getLanguage = function(layout) {
    var languageAndRegion = layout.split('-');
    switch (languageAndRegion.length) {
      case 1:
      case 2:
        return languageAndRegion[0];
      default:
        return '';
    }
  };

  /** @param {Object<string>} storage */
  var chooseLayout = function(storage) {
    var configuredLayout = storage[remoting.KeyboardLayoutsMenu.KEY_];
    var tryLayouts = [ chrome.i18n.getUILanguage() ];
    if (configuredLayout && typeof(configuredLayout) == 'string') {
      tryLayouts.unshift(configuredLayout);
    }
    for (var i = 0; i < tryLayouts.length; ++i) {
      if (layouts.indexOf(tryLayouts[i]) != -1) {
        onDone(tryLayouts[i]);
        return;
      }
      var language = getLanguage(tryLayouts[i]);
      if (language) {
        for (var j = 0; j < layouts.length; ++j) {
          if (language == getLanguage(layouts[j])) {
            onDone(layouts[j]);
            return;
          }
        }
      }
    }
    // Neither the stored layout nor UI locale was suitable.
    onDone(currentHostLayout);
  };

  chrome.storage.local.get(remoting.KeyboardLayoutsMenu.KEY_, chooseLayout);
};

/**
 * Create a menu id from the given keyboard layout.
 *
 * @param {string} layout Keyboard layout
 * @return {string}
 * @private
 */
remoting.KeyboardLayoutsMenu.prototype.makeMenuId_ = function(layout) {
  return 'layout@' + layout;
};

/**
 * Handle a click on the application's context menu.
 *
 * @param {OnClickData=} info
 * @private
 */
remoting.KeyboardLayoutsMenu.prototype.onContextMenu_ = function(info) {
  /** @type {Array<string>} */
  var components = info.menuItemId.split('@');
  if (components.length == 2 &&
      this.makeMenuId_(components[1]) == info.menuItemId) {
    this.setLayout_(true, components[1]);
  }
};

/**
 * @type {string}
 * @private
 */
remoting.KeyboardLayoutsMenu.KEY_ = 'preferred-keyboard-layout';
