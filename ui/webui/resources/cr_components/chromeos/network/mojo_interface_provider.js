// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('network_config', function() {
  /** @interface */
  class MojoInterfaceProvider {
    /** @return {!chromeos.networkConfig.mojom.CrosNetworkConfigProxy} */
    getMojoServiceProxy() {}
  }

  /** @implements {network_config.MojoInterfaceProvider} */
  class MojoInterfaceProviderImpl {
    constructor() {
      /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigProxy} */
      this.proxy_ = null;
    }

    /** @override */
    getMojoServiceProxy() {
      if (!this.proxy_) {
        this.proxy_ = chromeos.networkConfig.mojom.CrosNetworkConfig.getProxy();
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
