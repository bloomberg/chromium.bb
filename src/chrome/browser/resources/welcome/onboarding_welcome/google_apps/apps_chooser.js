// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @const
 */
var nuxGoogleApps = nuxGoogleApps || {};

/**
 * @typedef {{
 *   item: !{
 *     name: string,
 *     icon: string,
 *     selected: boolean,
 *   },
 *   set: function(string, boolean):void
 * }}
 */
nuxGoogleApps.AppsArrayModel;

Polymer({
  is: 'apps-chooser',
  properties: {
    // TODO(hcarmona): Get this list dynamically.
    appList: {
      type: Array,
      value: function() {
        return [
          {
            name: 'Gmail',
            icon: 'gmail',
            selected: true,
          },
          {
            name: 'YouTube',
            icon: 'youtube',
            selected: true,
          },
          {
            name: 'Maps',
            icon: 'maps',
            selected: true,
          },
          {
            name: 'Translate',
            icon: 'translate',
            selected: true,
          },
          {
            name: 'News',
            icon: 'news',
            selected: true,
          },
          {
            name: 'Web Store',
            icon: 'chrome-store',
            selected: true,
          },
        ];
      },
    },

    hasAppsSelected: {
      type: Boolean,
      notify: true,
      value: true,
    }
  },

  /**
   * Returns an array of booleans for each selected app.
   * @return {Array<boolean>}
   */
  getSelectedAppList() {
    return this.appList.map(a => a.selected);
  },

  /**
   * Handle toggling the apps selected.
   * @param {!{model: !nuxGoogleApps.AppsArrayModel}} e
   * @private
   */
  onAppClick_: function(e) {
    e.model.set('item.selected', !e.model.item.selected);
    this.hasAppsSelected = this.computeHasAppsSelected_();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAppPointerDown_: function(e) {
    e.currentTarget.classList.remove('keyboard-focused');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAppKeyUp_: function(e) {
    e.currentTarget.classList.add('keyboard-focused');
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasAppsSelected_: function() {
    return this.appList.some(a => a.selected);
  },
});
