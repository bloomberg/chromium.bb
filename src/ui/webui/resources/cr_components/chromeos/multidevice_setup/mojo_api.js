// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('multidevice_setup', function() {
  /** @interface */
  class MojoInterfaceProvider {
    /**
     * @return {!chromeos.multideviceSetup.mojom.MultiDeviceSetupProxy}
     */
    getMojoServiceProxy() {}
  }

  /** @implements {multidevice_setup.MojoInterfaceProvider} */
  class MojoInterfaceProviderImpl {
    constructor() {
      /** @private {?chromeos.multideviceSetup.mojom.MultiDeviceSetupProxy} */
      this.proxy_ = null;
    }

    /** @override */
    getMojoServiceProxy() {
      if (!this.proxy_) {
        this.proxy_ =
            chromeos.multideviceSetup.mojom.MultiDeviceSetup.getProxy();
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
