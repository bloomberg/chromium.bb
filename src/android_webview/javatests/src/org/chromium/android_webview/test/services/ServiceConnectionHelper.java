// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

import com.google.common.util.concurrent.SettableFuture;

import org.junit.Assert;

import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.base.ContextUtils;

/**
 * An abstraction to manage Service connections with the "try-with-resources" pattern. Instantiate
 * this class to initiate a binding to the specified Service. This will automatically unbind when
 * the try-block exits.
 *
 * <p>This must never be instantiated on the main Looper.
 */
public class ServiceConnectionHelper implements AutoCloseable {
    final SettableFuture<IBinder> mFuture = SettableFuture.create();
    final ServiceConnection mConnection;

    /**
     * Connects to a Service specified by {@code intent} with {@code flags}.
     *
     * @param intent should specify a Service.
     * @param flags should be {@code 0} or a combination of {@code Context#BIND_*}.
     */
    public ServiceConnectionHelper(Intent intent, int flags) {
        mConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mFuture.set(service);
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };

        Assert.assertTrue("Failed to bind to service",
                ContextUtils.getApplicationContext().bindService(intent, mConnection, flags));
    }

    /**
     * Returns the IBinder for this connection.
     */
    public IBinder getBinder() {
        return AwActivityTestRule.waitForFuture(mFuture);
    }

    @Override
    public void close() {
        ContextUtils.getApplicationContext().unbindService(mConnection);
    }
}
