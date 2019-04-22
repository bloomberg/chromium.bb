// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'plugin-vm-page' is the settings page for Plugin VM.
 */

Polymer({
  is: 'settings-plugin-vm-page',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.PLUGIN_VM_DETAILS) {
          map.set(settings.routes.PLUGIN_VM_DETAILS.path, '#pluginVmRow');
        }
        return map;
      },
    },
  },

  /** @private */
  onSubpageClick_: function(event) {
    settings.navigateTo(settings.routes.PLUGIN_VM_DETAILS);
  },
});
