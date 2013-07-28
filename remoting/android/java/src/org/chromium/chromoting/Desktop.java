// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Bundle;

import org.chromium.chromoting.jni.JniInterface;

/**
 * A simple screen that does nothing except display a DesktopView and notify it of rotations.
 */
public class Desktop extends Activity {
    /** Whether the device has just been rotated. */
    private DesktopView remoteHostDesktop;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        remoteHostDesktop = new DesktopView(this);
        setContentView(remoteHostDesktop);
    }

    /** Called when the activity is finally finished. */
    @Override
    public void onDestroy() {
        super.onDestroy();
        JniInterface.disconnectFromHost();
    }

    /** Called when the display is rotated (as registered in the manifest). */
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        remoteHostDesktop.requestCanvasRedraw();
    }
}
