// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-permission-view-header',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /** @type {App} */
    app_: {
      type: Object,
    },
  },

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    return app_management.util.getAppIcon(app);
  },

  /**
   * @private
   */
  onClickBackButton_: function() {
    if (!window.history.state) {
      this.dispatch(app_management.actions.changePage(PageType.MAIN));
    } else {
      window.history.back();
    }
  },

  /**
   * @private
   */
  onClickUninstallButton_: function() {
    app_management.BrowserProxy.getInstance().handler.uninstall(this.app_.id);
  },
});
