// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  // Not "toolbar" because element names must contain a hyphen.
  is: 'os-toolbar',

  properties: {
    // Value is proxied through to cr-toolbar-search-field. When true,
    // the search field will show a processing spinner.
    spinnerActive: Boolean,

    // Controls whether the menu button is shown at the start of the menu.
    showMenu: {type: Boolean, value: false},

    // Controls whether the search field is shown.
    showSearch: {type: Boolean, value: true},

    // True when the toolbar is displaying in narrow mode.
    narrow: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @private */
    showingSearch_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private */
    newOsSettingsSearch_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('newOsSettingsSearch');
      },
      readOnly: true,
    },
  },

  /** @return {?CrToolbarSearchFieldElement} */
  getSearchField() {
    if (this.newOsSettingsSearch_) {
      return /** @type {?CrToolbarSearchFieldElement} */ (
          this.shadowRoot.querySelector('os-settings-search-box')
              .$$('cr-toolbar-search-field'));
    }
    // TODO(crbug/1080777): Remove when new settings search complete.
    return /** @type {?CrToolbarSearchFieldElement} */ (
        this.$$('cr-toolbar-search-field'));
  },

  /** @private */
  onMenuTap_() {
    this.fire('os-toolbar-menu-tap');
  },
});
