// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Class used to forward view, input events down to native.
 *
 * TODO(jinsukkim): Add its native counterpart inheriting from ViewAndroid
 *     so that it will act as a root of ViewAndroid tree. It effectively
 *     replaces WindowAndroid in terms of the role.
 */
@JNINamespace("ui")
public class ViewRoot {
    // The corresponding native ViewAndroid. This object can only be used while
    // the native instance is alive.
    private long mNativeView;

    @CalledByNative
    private static ViewRoot create(long nativeView) {
        return new ViewRoot(nativeView);
    }

    private ViewRoot(long nativeView) {
        mNativeView = nativeView;
    }

    /**
     * Called when the underlying surface the compositor draws to changes size.
     * This may be larger than the viewport size.
     * @param width Width of the physical backing surface.
     * @param height Height of the physical backing surface.
     */
    public void onPhysicalBackingSizeChanged(int width, int height) {
        assert mNativeView != 0;
        nativeOnPhysicalBackingSizeChanged(mNativeView, width, height);
    }

    @CalledByNative
    private void onDestroyNativeView() {
        mNativeView = 0;
    }

    private static native void nativeOnPhysicalBackingSizeChanged(long viewAndroid,
            int width, int height);
}
