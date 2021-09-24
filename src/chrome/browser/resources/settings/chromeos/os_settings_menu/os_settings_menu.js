// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-selector/iron-selector.js';
import '../../settings_shared_css.js';
import '../os_icons.m.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, RouteObserverBehavior, Router} from '../../router.js';
import {routes} from '../os_route.m.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-menu',

  behaviors: [RouteObserverBehavior],

  properties: {
    advancedOpened: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Whether the user is in guest mode.
     * @private{boolean}
     */
    isGuestMode_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isGuest'),
      readOnly: true,
    },

    showCrostini: Boolean,

    showStartup: Boolean,

    showReset: Boolean,

    showKerberosSection: Boolean,

    /**
     * True if redesign of account management flows is enabled.
     * @private
     */
    isAccountManagementFlowsV2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled');
      },
      readOnly: true,
    },
  },

  /** @param {!Route} newRoute */
  currentRouteChanged(newRoute) {
    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('search');
    // If the route navigated to by a search result is in the advanced
    // section, the advanced menu will expand.
    if (urlSearchQuery && routes.ADVANCED &&
        routes.ADVANCED.contains(newRoute)) {
      this.advancedOpened = true;
    }

    // Focus the initially selected path.
    const anchors = this.root.querySelectorAll('a');
    for (let i = 0; i < anchors.length; ++i) {
      const anchorRoute =
          Router.getInstance().getRouteForPath(anchors[i].getAttribute('href'));
      if (anchorRoute && anchorRoute.contains(newRoute)) {
        this.setSelectedUrl_(anchors[i].href);
        return;
      }
    }

    this.setSelectedUrl_('');  // Nothing is selected.
  },

  /** @private */
  onAdvancedButtonToggle_() {
    this.advancedOpened = !this.advancedOpened;
  },

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately by <iron-selector>.
   * @param {!Event} event
   * @private
   */
  onLinkClick_(event) {
    if (event.target.matches('a')) {
      event.preventDefault();
    }
  },

  /**
   * Keeps both menus in sync. |url| needs to come from |element.href| because
   * |iron-list| uses the entire url. Using |getAttribute| will not work.
   * @param {string} url
   */
  setSelectedUrl_(url) {
    this.$.topMenu.selected = this.$.subMenu.selected = url;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSelectorActivate_(event) {
    this.setSelectedUrl_(event.detail.selected);
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Which icon to use.
   * @private
   * */
  arrowState_(opened) {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  },

  /** @return {boolean} Whether the advanced submenu is open. */
  isAdvancedSubmenuOpenedForTest() {
    const submenu = /** @type {IronCollapseElement} */ (this.$.advancedSubmenu);
    return submenu.opened;
  },

  /**
   * @param {boolean} bool
   * @return {string}
   * @private
   */
  boolToString_(bool) {
    return bool.toString();
  },
});
