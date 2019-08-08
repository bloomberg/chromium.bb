// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-export-import' is the settings backup and restore subpage for
 * Crostini.
 */

Polymer({
  is: 'settings-crostini-export-import',

  /** @private */
  onExportClick_: function() {
    settings.CrostiniBrowserProxyImpl.getInstance().exportCrostiniContainer();
  },

  /** @private */
  onImportClick_: function() {
    settings.CrostiniBrowserProxyImpl.getInstance().importCrostiniContainer();
  },
});
