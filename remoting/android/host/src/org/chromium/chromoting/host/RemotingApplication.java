// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.host;

import android.app.Application;

import org.chromium.chromoting.host.jni.Host;

/** Main context for the application. */
public class RemotingApplication extends Application {
    @Override
    public void onCreate() {
        Host.loadLibrary(this);
    }
}
