// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handler of the background page for the Drive sync events. Implementations
 * of this interface must @extends {cr.EventTarget}.
 *
 * @interface
 * @extends {EventTarget}
 */
function DriveSyncHandler() {}

DriveSyncHandler.prototype = /** @struct */ {
  __proto__: EventTarget.prototype,

  /**
   * @return {boolean} Whether the handler is syncing items or not.
   */
  get syncing() {}
};

/**
 * Returns the completed event name.
 * @return {string}
 */
DriveSyncHandler.prototype.getCompletedEventName = function() {};

/**
 * Returns whether the Drive sync is currently suppressed or not.
 * @return {boolean}
 */
DriveSyncHandler.prototype.isSyncSuppressed = function() {};

/**
 * Shows a notification that Drive sync is disabled on cellular networks.
 */
DriveSyncHandler.prototype.showDisabledMobileSyncNotification = function() {};
