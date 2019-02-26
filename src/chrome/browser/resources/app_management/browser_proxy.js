// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('app_management', function() {
  class BrowserProxy {
    constructor() {
      /** @type {appManagement.mojom.PageCallbackRouter} */
      this.callbackRouter = new appManagement.mojom.PageCallbackRouter();
      /** @type {appManagement.mojom.PageHandlerProxy} */
      this.handler = new appManagement.mojom.PageHandlerProxy();
      const factory = appManagement.mojom.PageHandlerFactory.getProxy();
      factory.createPageHandler(
          this.callbackRouter.createProxy(), this.handler.createRequest());
    }
  }

  cr.addSingletonGetter(BrowserProxy);

  return {BrowserProxy: BrowserProxy};
});
