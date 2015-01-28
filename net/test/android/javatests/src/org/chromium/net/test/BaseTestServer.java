// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import java.util.concurrent.atomic.AtomicBoolean;

/** A base class for simple test servers. */
public abstract class BaseTestServer implements Runnable {
    private AtomicBoolean mKeepRunning;

    /** Creates a test server. */
    public BaseTestServer() {
        mKeepRunning = new AtomicBoolean(true);
    }

    /** Accepts incoming connections until stopped via stop(). */
    public void run() {
        mKeepRunning.set(true);

        while (mKeepRunning.get()) {
            accept();
        }
    }

    /** Waits for and handles an incoming request. */
    protected abstract void accept();

    /** Returns the port on which this server is listening for connections. */
    public abstract int getServerPort();

    /** Stops the server. */
    public void stop() {
        mKeepRunning.set(false);
    }
}
