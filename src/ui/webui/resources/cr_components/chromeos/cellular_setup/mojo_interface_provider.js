// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cellular_setup', function() {
  /** @interface */
  class MojoInterfaceProvider {
    /** @return {!chromeos.cellularSetup.mojom.CellularSetupProxy} */
    getMojoServiceProxy() {}
  }

  /** @implements {cellular_setup.MojoInterfaceProvider} */
  class MojoInterfaceProviderImpl {
    constructor() {
      /** @private {?chromeos.cellularSetup.mojom.CellularSetupProxy} */
      this.proxy_ = null;
    }

    /** @override */
    getMojoServiceProxy() {
      if (!this.proxy_) {
        this.proxy_ = chromeos.cellularSetup.mojom.CellularSetup.getProxy();
      }

      return this.proxy_;
    }
  }

  cr.addSingletonGetter(MojoInterfaceProviderImpl);

  return {
    MojoInterfaceProvider: MojoInterfaceProvider,
    MojoInterfaceProviderImpl: MojoInterfaceProviderImpl,
  };
});
