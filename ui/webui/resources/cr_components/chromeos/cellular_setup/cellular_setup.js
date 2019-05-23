// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Root element for the cellular setup flow. This element interacts with the
 * CellularSetup service to carry out the activation flow. It contains
 * navigation buttons and sub-pages corresponding to each step of the flow.
 */
Polymer({
  is: 'cellular-setup',

  behaviors: [I18nBehavior],

  listeners: {
    'backward-nav-requested': 'onBackwardNavRequested_',
    'retry-requested': 'onRetryRequested_',
    'complete-flow-requested': 'onCompleteFlowRequested_',
  },

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

  /** @private */
  onBackwardNavRequested_: function() {
    // TODO(azeemarshad): Add back navigation.
  },

  /** @private */
  onRetryRequested_: function() {
    // TODO(azeemarshad): Add try again logic.
  },

  /** @private */
  onCompleteFlowRequested__: function() {
    // TODO(azeemarshad): Add completion logic.
  },
});
