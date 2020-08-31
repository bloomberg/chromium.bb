// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-plugin-vm-detail-view',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: Object,
  },

  attached() {
    // When the state is changed, get the new selected app and assign it to
    // |app_|
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * @private
   */
  onSharedPathsClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS);
  },
});
