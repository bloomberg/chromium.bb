// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-subpage' is the settings subpage for managing Crostini.
 */

Polymer({
  is: 'settings-crostini-subpage',

  behaviors: [PrefsBehavior, WebUIListenerBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether CrostiniUsbSupport flag is enabled.
     * @private {boolean}
     */
    enableCrostiniUsbDeviceSupport_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableCrostiniUsbDeviceSupport');
      },
    },

    /**
     * Whether export / import UI should be displayed.
     * @private {boolean}
     */
    showCrostiniExportImport_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showCrostiniExportImport');
      },
    },

    /**
     * Whether the uninstall options should be displayed.
     * @private {boolean}
     */
    hideCrostiniUninstall_: {
      type: Boolean,
    },
  },

  observers: ['onCrostiniEnabledChanged_(prefs.crostini.enabled.value)'],

  attached: function() {
    const callback = (status) => {
      this.hideCrostiniUninstall_ = status;
    };
    this.addWebUIListener('crostini-installer-status-changed', callback);
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestCrostiniInstallerStatus();
  },

  /** @private */
  onCrostiniEnabledChanged_: function(enabled) {
    if (!enabled &&
        settings.getCurrentRoute() == settings.routes.CROSTINI_DETAILS) {
      settings.navigateToPreviousRoute();
    }
  },

  /** @private */
  onExportImportClick_: function() {
    settings.navigateTo(settings.routes.CROSTINI_EXPORT_IMPORT);
  },

  /**
   * Shows a confirmation dialog when removing crostini.
   * @private
   */
  onRemoveClick_: function() {
    settings.CrostiniBrowserProxyImpl.getInstance().requestRemoveCrostini();
  },

  /** @private */
  onSharedPathsClick_: function() {
    settings.navigateTo(settings.routes.CROSTINI_SHARED_PATHS);
  },

  /** @private */
  onSharedUsbDevicesClick_: function() {
    settings.navigateTo(settings.routes.CROSTINI_SHARED_USB_DEVICES);
  },
});
