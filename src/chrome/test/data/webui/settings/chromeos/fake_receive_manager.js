// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

/**
 * @fileoverview Fake implementation of ReceiveManagerInterface for testing.
 */
cr.define('nearby_share', function() {
  /**
   * Fake implementation of nearbyShare.mojom.ReceiveManagerInterface
   *
   * @implements {nearbyShare.mojom.ReceiveManagerInterface}
   */
  /* #export */ class FakeReceiveManager extends TestBrowserProxy {
    constructor() {
      super([
        'addReceiveObserver',
        'isInHighVisibility',
        'registerForegroundReceiveSurface',
        'unregisterForegroundReceiveSurface',
        'accept',
        'reject',
      ]);
      /** @private {!nearbyShare.mojom.ReceiveManagerObserverInterface} */
      this.observer_;
      /** @private {!boolean} */
      this.inHighVisibility_ = false;
      /** @private {?mojoBase.mojom.UnguessableToken} */
      this.lastToken_ = null;
      /** @private {!boolean} */
      this.nextResult_ = true;
      // Make this look like a closable mojo pipe
      /** @private {Object} */
      this.$ = {
        close() {},
      };
    }

    simulateShareTargetArrival(name, connectionToken) {
      const target = {id: {low: 1, high: 2}, name: name, type: 1};
      const metadata = {
        'status': nearbyShare.mojom.TransferStatus.kAwaitingLocalConfirmation,
        progress: 0.0,
        token: connectionToken,
        is_original: true,
        is_final_status: false
      };
      this.observer_.onTransferUpdate(target, metadata);
      return target;
    }

    /**
     * @param {!nearbyShare.mojom.ReceiveObserverRemote} observer
     */
    addReceiveObserver(observer) {
      this.methodCalled('addReceiveObserver');
      this.observer_ = observer;
    }

    /**
     * @return {!Promise<{inHighVisibility: !boolean}>}
     */
    async isInHighVisibility() {
      this.methodCalled('isInHighVisibility');
      return {inHighVisibility: this.inHighVisibility_};
    }

    /**
     * @return {!Promise<{success: !boolean}>}
     */
    async registerForegroundReceiveSurface() {
      this.inHighVisibility_ = true;
      if (this.observer_) {
        this.observer_.onHighVisibilityChanged(this.inHighVisibility_);
      }
      this.methodCalled('registerForegroundReceiveSurface');
      return {success: this.nextResult_};
    }

    /**
     * @return {!Promise<{success: !boolean}>}
     */
    async unregisterForegroundReceiveSurface() {
      this.inHighVisibility_ = false;
      if (this.observer_) {
        this.observer_.onHighVisibilityChanged(this.inHighVisibility_);
      }
      this.methodCalled('unregisterForegroundReceiveSurface');
      return {success: this.nextResult_};
    }

    /**
     * @param {!mojoBase.mojom.UnguessableToken} shareTargetId
     * @return {!Promise<{success: !boolean}>}
     */
    async accept(shareTargetId) {
      this.lastToken_ = shareTargetId;
      this.methodCalled('accept', shareTargetId);
      return {success: this.nextResult_};
    }

    /**
     * @param {!mojoBase.mojom.UnguessableToken} shareTargetId
     * @return {!Promise<{success: !boolean}>}
     */
    async reject(shareTargetId) {
      this.lastToken_ = shareTargetId;
      this.methodCalled('reject', shareTargetId);
      return {success: this.nextResult_};
    }
  }

  // #cr_define_end
  return {FakeReceiveManager: FakeReceiveManager};
});
