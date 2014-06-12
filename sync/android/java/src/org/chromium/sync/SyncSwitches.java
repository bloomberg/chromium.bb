// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync;

/**
 * Contains all of the command line switches that are specific to the sync/
 * portion of Chromium on Android.
 */
public abstract class SyncSwitches {
    // It's currently necessary for people wanting to try out the Push API to disable Chrome Sync's
    // use of GCM (as Android only grants a single registration ID, and they can't both use it).
    // Accordingly, this switch lets you disable Sync's ability to receive GCM messages in order to
    // make it possible to try out the Push API. Don't use this in production!
    // TODO(johnme): Remove this command line switch once disabling sync is no longer necessary.
    public static final String DISABLE_SYNC_GCM_IN_ORDER_TO_TRY_PUSH_API =
            "disable-sync-gcm-in-order-to-try-push-api";

    // Prevent instantiation.
    private SyncSwitches() {}
}
