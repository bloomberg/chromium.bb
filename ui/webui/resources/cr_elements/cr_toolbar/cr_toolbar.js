// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-toolbar',

  properties: {
    // Name to display in the toolbar, in titlecase.
    pageName: String,

    // Prompt text to display in the search field.
    searchPrompt: String,

    // Tooltip to display on the clear search button.
    clearLabel: String,

    // Tooltip to display on the menu button.
    menuLabel: String,

    // Promotional toolstip string, shown in narrow mode if showMenuPromo is
    // true.
    menuPromo: String,

    // Value is proxied through to cr-toolbar-search-field. When true,
    // the search field will show a processing spinner.
    spinnerActive: Boolean,

    // Controls whether the menu button is shown at the start of the menu.
    showMenu: {type: Boolean, value: false},

    // Whether to show menu promo tooltip.
    showMenuPromo: {
      type: Boolean,
      value: false,
    },

    // True when the toolbar is displaying in narrow mode.
    narrow: {
      type: Boolean,
      reflectToAttribute: true,
      readonly: true,
      notify: true,
    },

    closeMenuPromo: String,

    /** @private */
    showingSearch_: {
      type: Boolean,
      reflectToAttribute: true,
    },
  },

  observers: [
    'possiblyShowMenuPromo_(showMenu, showMenuPromo, showingSearch_)',
  ],

  /** @return {!CrToolbarSearchFieldElement} */
  getSearchField: function() {
    return this.$.search;
  },

  /** @private */
  onClosePromoTap_: function() {
    this.fire('cr-toolbar-menu-promo-close');
  },

  /** @private */
  onMenuTap_: function() {
    this.fire('cr-toolbar-menu-tap');
  },

  /** @private */
  possiblyShowMenuPromo_: function() {
    Polymer.RenderStatus.afterNextRender(this, function() {
      if (this.showMenu && this.showMenuPromo && !this.showingSearch_) {
        this.$$('#menuPromo')
            .animate(
                {
                  opacity: [0, .9],
                },
                /** @type {!KeyframeEffectOptions} */ ({
                  duration: 500,
                  fill: 'forwards'
                }));
        this.fire('cr-toolbar-menu-promo-shown');
      }
    }.bind(this));
  },

  /**
   * @param {string} title
   * @param {boolean} showMenuPromo
   * @return {string} The title if the menu promo isn't showing, else "".
   */
  titleIfNotShowMenuPromo_: function(title, showMenuPromo) {
    return showMenuPromo ? '' : title;
  },
});
