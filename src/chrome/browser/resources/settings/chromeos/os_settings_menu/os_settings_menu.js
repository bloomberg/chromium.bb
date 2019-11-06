// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
Polymer({
  is: 'os-settings-menu',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
    advancedOpened: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!PageVisibility}
     */
    pageVisibility: Object,
  },

  /** @param {!settings.Route} newRoute */
  currentRouteChanged: function(newRoute) {
    const currentPath = newRoute.path;

    // Focus the initially selected path.
    const anchors = this.root.querySelectorAll('a');
    for (let i = 0; i < anchors.length; ++i) {
      if (anchors[i].getAttribute('href') == currentPath) {
        this.setSelectedUrl_(anchors[i].href);
        return;
      }
    }

    this.setSelectedUrl_('');  // Nothing is selected.
  },

  /** @private */
  onAdvancedButtonToggle_: function() {
    this.advancedOpened = !this.advancedOpened;
  },

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately by <iron-selector>.
   * @param {!Event} event
   * @private
   */
  onLinkClick_: function(event) {
    if (event.target.matches('a')) {
      event.preventDefault();
    }
  },

  /**
   * Keeps both menus in sync. |url| needs to come from |element.href| because
   * |iron-list| uses the entire url. Using |getAttribute| will not work.
   * @param {string} url
   */
  setSelectedUrl_: function(url) {
    this.$.topMenu.selected = this.$.subMenu.selected = url;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSelectorActivate_: function(event) {
    this.setSelectedUrl_(event.detail.selected);

    const path = new URL(event.detail.selected).pathname;
    const route = settings.getRouteForPath(path);
    assert(route, 'os-settings-menu has an entry with an invalid route.');
    settings.navigateTo(
        route, /* dynamicParams */ null, /* removeSearch */ true);
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Which icon to use.
   * @private
   * */
  arrowState_: function(opened) {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  },

  /** @return {boolean} Whether the advanced submenu is open. */
  isAdvancedSubmenuOpenedForTest: function() {
    const submenu = /** @type {IronCollapseElement} */ (this.$.advancedSubmenu);
    return submenu.opened;
  },
});
