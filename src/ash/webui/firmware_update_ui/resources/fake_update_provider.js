// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';

import {FirmwareUpdate, UpdateObserver, UpdateProviderInterface} from './firmware_update_types.js';

// Method names.
export const ON_UPDATE_LIST_CHANGED = 'UpdateObserver_onUpdateListChanged';

/**
 * @fileoverview
 * Implements a fake version of the UpdateProvider mojo interface.
 */

/** @implements {UpdateProviderInterface} */
export class FakeUpdateProvider {
  constructor() {
    this.observables_ = new FakeObservables();

    /** @private {?Promise} */
    this.observePeripheralUpdatesPromise_ = null;

    this.registerObservables();
  }

  /*
   * Implements UpdateProviderInterface.ObservePeripheralUpdates.
   * @param {!UpdateObserver} remote
   * @return {!Promise}
   */
  observePeripheralUpdates(remote) {
    this.observePeripheralUpdatesPromise_ =
        this.observe_(ON_UPDATE_LIST_CHANGED, (firmwareUpdates) => {
          remote.onUpdateListChanged(firmwareUpdates);
        });
  }

  /**
   * Sets the values that will be observed from observePeripheralUpdates.
   * @param {!Array<!Array<!FirmwareUpdate>>} firmwareUpdates
   */
  setFakeFirmwareUpdates(firmwareUpdates) {
    this.observables_.setObservableData(
        ON_UPDATE_LIST_CHANGED, [firmwareUpdates]);
  }

  /**
   * Returns the promise for the most recent peripheral updates observation.
   * @return {?Promise}
   */
  getObservePeripheralUpdatesPromiseForTesting() {
    return this.observePeripheralUpdatesPromise_;
  }

  /**
   * Causes the device added observer to fire.
   */
  triggerDeviceAddedObserver() {
    this.observables_.trigger(ON_UPDATE_LIST_CHANGED);
  }

  registerObservables() {
    this.observables_.register(ON_UPDATE_LIST_CHANGED);
  }

  /**
   * Disables all observers and resets provider to its initial state.
   */
  reset() {
    this.observables_ = new FakeObservables();
    this.registerObservables();
  }

  /**
   * Sets up an observer for methodName.
   * @template T
   * @param {string} methodName
   * @param {!function(!T)} callback
   * @return {!Promise}
   * @private
   */
  observe_(methodName, callback) {
    return new Promise((resolve) => {
      this.observables_.observe(methodName, callback);
      this.observables_.trigger(methodName);
      resolve();
    });
  }
}
