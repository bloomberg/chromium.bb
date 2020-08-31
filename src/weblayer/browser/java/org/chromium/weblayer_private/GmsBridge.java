// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.Handler;
import android.os.HandlerThread;

import org.chromium.base.Callback;
import org.chromium.base.Consumer;
import org.chromium.base.ThreadUtils;
import org.chromium.components.metrics.AndroidMetricsLogUploader;

/**
 * This class manages functionality related to Google Mobile Services (i.e. GMS).
 * Platform specific implementations are provided in GmsBridgeImpl.java.
 */
public abstract class GmsBridge {
    private static GmsBridge sInstance;
    private static final Object sInstanceLock = new Object();

    private static HandlerThread sHandlerThread;
    private static Handler sHandler;
    private static final Object sHandlerLock = new Object();

    protected GmsBridge() {
        AndroidMetricsLogUploader.setUploader(new Consumer<byte[]>() {
            @Override
            public void accept(byte[] data) {
                logMetrics(data);
            }
        });
    }

    public static GmsBridge getInstance() {
        synchronized (sInstanceLock) {
            if (sInstance == null) {
                // Load an instance of GmsBridgeImpl. Because this can change depending on
                // the GN configuration, this may not be the GmsBridgeImpl defined upstream.
                sInstance = new GmsBridgeImpl();
            }
            return sInstance;
        }
    }

    // Provide a mocked GmsBridge for testing.
    public static void injectInstance(GmsBridge testBridge) {
        synchronized (sInstanceLock) {
            sInstance = testBridge;
        }
    }

    // Return a handler appropriate for executing blocking Platform Service tasks.
    public static Handler getHandler() {
        synchronized (sHandlerLock) {
            if (sHandler == null) {
                sHandlerThread = new HandlerThread("GmsBridgeHandlerThread");
                sHandlerThread.start();
                sHandler = new Handler(sHandlerThread.getLooper());
            }
        }
        return sHandler;
    }

    // Returns true if the WebLayer can use Google Mobile Services (GMS).
    public boolean canUseGms() {
        return false;
    }

    public void setSafeBrowsingHandler() {
        // We don't have this specialized service here.
    }

    // Overriding implementations may call "callback" asynchronously. For simplicity (and not
    // because of any technical limitation) we require that "queryMetricsSetting" and "callback"
    // both get called on WebLayer's UI thread.
    public void queryMetricsSetting(Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        callback.onResult(false);
    }

    public void logMetrics(byte[] data) {}
}
