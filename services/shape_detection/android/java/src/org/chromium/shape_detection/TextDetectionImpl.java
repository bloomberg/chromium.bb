// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Rect;
import android.util.SparseArray;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.vision.Frame;
import com.google.android.gms.vision.text.TextBlock;
import com.google.android.gms.vision.text.TextRecognizer;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.gfx.mojom.RectF;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.SharedBufferHandle;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.shape_detection.mojom.TextDetection;
import org.chromium.shape_detection.mojom.TextDetectionResult;


/**
 * Implementation of mojo TextDetection, using Google Play Services vision package.
 */
public class TextDetectionImpl implements TextDetection {
    private static final String TAG = "TextDetectionImpl";

    private TextRecognizer mTextRecognizer;

    public TextDetectionImpl() {
        mTextRecognizer = new TextRecognizer.Builder(ContextUtils.getApplicationContext()).build();
    }

    @Override
    public void detect(
            SharedBufferHandle frameData, int width, int height, DetectResponse callback) {
        if (GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(
                    ContextUtils.getApplicationContext())
                != ConnectionResult.SUCCESS) {
            Log.e(TAG, "Google Play Services not available");
            callback.call(new TextDetectionResult[0]);
            return;
        }
        // The vision library will be downloaded the first time the API is used
        // on the device; this happens "fast", but it might have not completed,
        // bail in this case. Also, the API was disabled between and v.9.0 and
        // v.9.2, see https://developers.google.com/android/guides/releases.
        if (!mTextRecognizer.isOperational()) {
            Log.e(TAG, "TextDetector is not operational");
            callback.call(new TextDetectionResult[0]);
            return;
        }

        Frame frame = SharedBufferUtils.convertToFrame(frameData, width, height);
        if (frame == null) {
            Log.e(TAG, "Error converting SharedMemory to Frame");
            callback.call(new TextDetectionResult[0]);
            return;
        }

        final SparseArray<TextBlock> textBlocks = mTextRecognizer.detect(frame);

        TextDetectionResult[] detectedTextArray = new TextDetectionResult[textBlocks.size()];
        for (int i = 0; i < textBlocks.size(); i++) {
            detectedTextArray[i] = new TextDetectionResult();
            detectedTextArray[i].rawValue = textBlocks.valueAt(i).getValue();
            final Rect rect = textBlocks.valueAt(i).getBoundingBox();
            detectedTextArray[i].boundingBox = new RectF();
            detectedTextArray[i].boundingBox.x = rect.left;
            detectedTextArray[i].boundingBox.y = rect.top;
            detectedTextArray[i].boundingBox.width = rect.width();
            detectedTextArray[i].boundingBox.height = rect.height();
        }
        callback.call(detectedTextArray);
    }

    @Override
    public void close() {
        mTextRecognizer.release();
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    /**
     * A factory class to register TextDetection interface.
     */
    public static class Factory implements InterfaceFactory<TextDetection> {
        public Factory() {}

        @Override
        public TextDetection createImpl() {
            return new TextDetectionImpl();
        }
    }
}
