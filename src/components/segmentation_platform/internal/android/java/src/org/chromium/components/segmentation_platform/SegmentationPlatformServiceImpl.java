// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.optimization_guide.proto.ModelsProto;
import org.chromium.components.optimization_guide.proto.ModelsProto.OptimizationTarget;

/**
 * Java side of the JNI bridge between SegmentationPlatformServiceImpl in Java
 * and C++. All method calls are delegated to the native C++ class.
 */
@JNINamespace("segmentation_platform")
public class SegmentationPlatformServiceImpl implements SegmentationPlatformService {
    private long mNativePtr;

    @CalledByNative
    private static SegmentationPlatformServiceImpl create(long nativePtr) {
        return new SegmentationPlatformServiceImpl(nativePtr);
    }

    private SegmentationPlatformServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public void getSelectedSegment(
            String segmentationKey, Callback<SegmentSelectionResult> callback) {
        SegmentationPlatformServiceImplJni.get().getSelectedSegment(
                mNativePtr, this, segmentationKey, callback);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @CalledByNative
    private static SegmentSelectionResult createSegmentSelectionResult(
            boolean isReady, int selectedSegment) {
        OptimizationTarget segment = ModelsProto.OptimizationTarget.forNumber(selectedSegment);
        if (segment == null) segment = OptimizationTarget.OPTIMIZATION_TARGET_UNKNOWN;
        return new SegmentSelectionResult(isReady, segment);
    }

    @NativeMethods
    interface Natives {
        void getSelectedSegment(long nativeSegmentationPlatformServiceAndroid,
                SegmentationPlatformServiceImpl caller, String segmentationKey,
                Callback<SegmentSelectionResult> callback);
    }
}
