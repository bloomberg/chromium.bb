// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gfx;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.JNINamespace;

/**
 * The window base class that has the minimum functionality.
 */
@JNINamespace("ui")
public abstract class NativeWindow {

    // Native pointer to the c++ WindowAndroid object.
    private int mNativeWindowAndroid = 0;

    private Context mContext;

    /**
     * An interface that intent callback objects have to implement.
     */
    public interface IntentCallback {
        /**
         * Handles the data returned by the requested intent.
         * @param window A window reference.
         * @param resultCode Result code of the requested intent.
         * @param contentResolver An instance of ContentResolver class for accessing returned data.
         * @param data The data returned by the intent.
         */
        public void onIntentCompleted(NativeWindow window, int resultCode,
                ContentResolver contentResolver, Intent data);
    }

    /**
     * Constructs a Window object, saves a reference to the context.
     * @param context
     */
    public NativeWindow(Context context) {
        mNativeWindowAndroid = 0;
        mContext = context;
    }

    /**
     * Stub overridden by extending class.
     */
    abstract public void sendBroadcast(Intent intent);

    /**
     * Stub overridden by extending class.
     */
    abstract public boolean showIntent(Intent intent, IntentCallback callback, String error);

    /**
     * Stub overridden by extending class.
     */
    abstract public void showError(String error);

    /**
     * @return context.
     */
    public Context getContext() {
        return mContext;
    }

    /**
     * Destroys the c++ WindowAndroid object if one has been created.
     */
    public void destroy() {
        if (mNativeWindowAndroid != 0) {
            nativeDestroy(mNativeWindowAndroid);
            mNativeWindowAndroid = 0;
        }
    }

    /**
     * Returns a pointer to the c++ AndroidWindow object and calls the initializer if
     * the object has not been previously initialized.
     * @return A pointer to the c++ AndroidWindow.
     */
    public int getNativePointer() {
        if (mNativeWindowAndroid == 0) {
            mNativeWindowAndroid = nativeInit();
        }
        return mNativeWindowAndroid;
    }

    private native int nativeInit();
    private native void nativeDestroy(int nativeWindowAndroid);

}
