// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {observeESimManager} from './mojo_interface_provider.m.js';

/**
 * @fileoverview Polymer behavior for observing ESimManagerObserver
 * events.
 */

/** @polymerBehavior */
/* #export */ const ESimManagerListenerBehavior = {
  /** @private {?ash.cellularSetup.mojom.ESimManagerObserver} */
  observer_: null,

  /** @override */
  attached() {
    cellular_setup.observeESimManager(this);
  },

  // ESimManagerObserver methods. Override these in the implementation.

  onAvailableEuiccListChanged() {},

  /**
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   */
  onProfileListChanged(euicc) {},

  /**
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   */
  onEuiccChanged(euicc) {},

  /**
   * @param {!ash.cellularSetup.mojom.ESimProfileRemote} profile
   */
  onProfileChanged(profile) {},
};

/** @interface */
/* #export */ class ESimManagerListenerBehaviorInterface {
  onAvailableEuiccListChanged() {}

  /**
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   */
  onProfileListChanged(euicc) {}

  /**
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   */
  onEuiccChanged(euicc) {}

  /**
   * @param {!ash.cellularSetup.mojom.ESimProfileRemote} profile
   */
  onProfileChanged(profile) {}
}
