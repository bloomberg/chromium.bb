// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-app',

  /** @override */
  attached: function() {
    app_management.BrowserProxy.getInstance().handler.getApps();
    let callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_ =
        [callbackRouter.onAppsAdded.addListener((id) => console.log(id))];
  },

  detached: function() {
    let callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach((id) => callbackRouter.removeListener(id));
  },
});
