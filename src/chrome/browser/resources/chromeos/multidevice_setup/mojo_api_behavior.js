// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @polymerBehavior */
const MojoApiBehavior = {
  properties: {
    /**
     * Provides access to the MultiDeviceSetup Mojo service.
     * @type {!chromeos.multideviceSetup.mojom.MultiDeviceSetupImpl}
     */
    multideviceSetup: {
      type: Object,
      value: function() {
        let ptr = new chromeos.multideviceSetup.mojom.MultiDeviceSetupPtr();
        Mojo.bindInterface(
            chromeos.multideviceSetup.mojom.MultiDeviceSetup.name,
            mojo.makeRequest(ptr).handle);
        return ptr;
      },
    }
  },
};
