// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.io.IOException;

/**
 * This class provides the resource bundle related methods for the native library.
 */
@JNINamespace("ui")
class ResourceBundle {
    @CalledByNative
    static boolean assetContainedInApk(Context ctx, String filename) {
        try {
            AssetManager am = ctx.getAssets();
            AssetFileDescriptor afd = am.openFd(filename);
            afd.close();
            return true;
        } catch (IOException e) {
            return false;
        }
    }
}
