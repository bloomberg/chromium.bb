/*
 * Copyright 2021 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

package org.skia.androidkit;

import org.skia.androidkit.Color;
import org.skia.androidkit.Matrix;
import org.skia.androidkit.Paint;
import org.skia.androidkit.Surface;

public class Canvas {
    private long mNativeInstance;
    private Surface mSurface;

    public int getWidth() {
        return nGetWidth(mNativeInstance);
    }

    public int getHeight() {
        return nGetHeight(mNativeInstance);
    }

    public void save() {
        nSave(mNativeInstance);
    }

    public void restore() {
        nRestore(mNativeInstance);
    }

    public Matrix getLocalToDevice() {
        long native_matrix = nGetLocalToDevice(mNativeInstance);
        return new Matrix(native_matrix);
    }

    public void concat(Matrix m) {
        nConcat(mNativeInstance, m.getNativeInstance());
    }

    public void concat(float[] rowMajorMatrix) {
        if (rowMajorMatrix.length != 16) {
            throw new java.lang.IllegalArgumentException("Expecting a 16 float array.");
        }

        nConcat16f(mNativeInstance, rowMajorMatrix);
    }

    public void drawRect(float left, float right, float top, float bottom, Paint paint) {
        nDrawRect(mNativeInstance, left, right, top, bottom, paint.getNativeInstance());
    }

    public void drawColor(Color c) {
        nDrawColor(mNativeInstance, c.r(), c.g(), c.b(), c.a());
    }

    public void drawColor(float r, float g, float b, float a) {
        nDrawColor(mNativeInstance, r, g, b, a);
    }

    public void drawColor(int icolor) {
        nDrawColor(mNativeInstance,
            (float)((icolor >> 16) & 0xff) / 255,
            (float)((icolor >>  8) & 0xff) / 255,
            (float)((icolor >>  0) & 0xff) / 255,
            (float)((icolor >> 24) & 0xff) / 255
        );
    }

    // package private
    Canvas(Surface surface, long native_instance) {
        mNativeInstance = native_instance;
        mSurface = surface;
    }

    // package private
    long getNativeInstance() { return mNativeInstance; }

    private static native int  nGetWidth(long nativeInstance);
    private static native int  nGetHeight(long nativeInstance);
    private static native void nSave(long nativeInstance);
    private static native void nRestore(long nativeInstance);
    private static native long nGetLocalToDevice(long mNativeInstance);
    private static native void nConcat(long nativeInstance, long nativeMatrix);
    private static native void nConcat16f(long nativeInstance, float[] floatMatrix);

    private static native void nDrawColor(long nativeInstance, float r, float g, float b, float a);

    private static native void nDrawRect(long nativeInstance,
                                         float left, float right, float top, float bottom,
                                         long nativePaint);
}
