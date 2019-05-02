// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(azeemarshad): Implement cellular setup UI.
Polymer({
  is: 'cellular-setup',

  behaviors: [I18nBehavior],

  /**
   * Provides an interface to the CellularSetup Mojo service.
   * @private {?cellular_setup.MojoInterfaceProvider}
   */
  mojoInterfaceProvider_: null,

  /** @override */
  created: function() {
    this.mojoInterfaceProvider_ =
        cellular_setup.MojoInterfaceProviderImpl.getInstance();
  },
});
