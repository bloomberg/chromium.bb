// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('app_management', function() {
  class BrowserProxy {
    constructor() {
      /** @type {appManagement.mojom.PageCallbackRouter} */
      this.callbackRouter = new appManagement.mojom.PageCallbackRouter();

      /** @type {appManagement.mojom.PageHandlerInterface} */
      this.handler = null;

      const urlParams = new URLSearchParams(window.location.search);
      const useFake = urlParams.get('fakeBackend');

      if (useFake) {
        this.handler = new app_management.FakePageHandler(
            this.callbackRouter.createProxy());

        const /** @type {!Array<App>}*/ appList = [
          app_management.FakePageHandler.createApp(
              'blpcfgokakmgnkcojhhkbfblekacnbeo',
              {
                title: 'Built in app, not implemented',
                type: AppType.kBuiltIn,
                installSource: InstallSource.kSystem,
              },
              ),
          app_management.FakePageHandler.createApp(
              'aohghmighlieiainnegkcijnfilokake',
              {
                title: 'Arc app',
                type: AppType.kArc,
                installSource: InstallSource.kUser,
              },
              ),
          app_management.FakePageHandler.createApp(
              'blpcfgokakmgnkcojhhkbfbldkacnbeo',
              {
                title: 'Crostini app, not implemented',
                type: AppType.kCrostini,
                installSource: InstallSource.kUser,
              },
              ),
          app_management.FakePageHandler.createApp(
              'pjkljhegncpnkkknowihdijeoejaedia',
              {
                title: 'Chrome App',
                type: AppType.kExtension,
              },
              ),
          app_management.FakePageHandler.createApp(
              'aapocclcdoekwnckovdopfmtonfmgok',
              {
                title: 'Web App',
                type: AppType.kWeb,
              },
              ),
          app_management.FakePageHandler.createApp(
              'pjkljhegncpnkkknbcohdijeoejaedia',
              {
                title: 'Chrome App, OEM installed',
                type: AppType.kExtension,
                installSource: InstallSource.kOem,
              },
              ),
          app_management.FakePageHandler.createApp(
              'aapocclcgogkmnckokdopfmhonfmgok',
              {
                title: 'Web App, policy installed',
                type: AppType.kWeb,
                installSource: InstallSource.kPolicy,
              },
              ),
        ];

        this.handler.setApps(appList);

      } else {
        this.handler = new appManagement.mojom.PageHandlerProxy();
        const factory = appManagement.mojom.PageHandlerFactory.getProxy();
        factory.createPageHandler(
            this.callbackRouter.createProxy(), this.handler.$.createRequest());
      }
    }
  }

  cr.addSingletonGetter(BrowserProxy);

  return {BrowserProxy: BrowserProxy};
});
