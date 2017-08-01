// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.Feature;
import org.chromium.shape_detection.mojom.FaceDetection;
import org.chromium.shape_detection.mojom.FaceDetectionResult;
import org.chromium.shape_detection.mojom.FaceDetectorOptions;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Test suite for FaceDetectionImpl.
 */
public class FaceDetectionImplTest extends InstrumentationTestCase {
    private static final org.chromium.skia.mojom.Bitmap MONA_LISA_BITMAP =
            TestUtils.mojoBitmapFromFile("mona_lisa.jpg");
    // Different versions of Android have different implementations of FaceDetector.findFaces(), so
    // we have to use a large error threshold.
    private static final double BOUNDING_BOX_POSITION_ERROR = 10.0;
    private static final double BOUNDING_BOX_SIZE_ERROR = 5.0;
    private static enum DetectionProviderType { ANDROID, GMS_CORE }

    public FaceDetectionImplTest() {}

    private static FaceDetectionResult[] detect(
            org.chromium.skia.mojom.Bitmap mojoBitmap, DetectionProviderType api) {
        FaceDetectorOptions options = new FaceDetectorOptions();
        options.fastMode = true;
        options.maxDetectedFaces = 32;
        FaceDetection detector = null;
        if (api == DetectionProviderType.ANDROID) {
            detector = new FaceDetectionImpl(options);
        } else if (api == DetectionProviderType.GMS_CORE) {
            detector = new FaceDetectionImplGmsCore(options);
        } else {
            assert false;
            return null;
        }

        final ArrayBlockingQueue<FaceDetectionResult[]> queue = new ArrayBlockingQueue<>(1);
        detector.detect(mojoBitmap, new FaceDetection.DetectResponse() {
            public void call(FaceDetectionResult[] results) {
                queue.add(results);
            }
        });
        FaceDetectionResult[] toReturn = null;
        try {
            toReturn = queue.poll(5L, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            fail("Could not get FaceDetectionResult: " + e.toString());
        }
        assertNotNull(toReturn);
        return toReturn;
    }

    private void detectSucceedsOnValidImage(DetectionProviderType api) {
        FaceDetectionResult[] results = detect(MONA_LISA_BITMAP, api);
        assertEquals(1, results.length);
        assertEquals(40.0, results[0].boundingBox.width, BOUNDING_BOX_SIZE_ERROR);
        assertEquals(40.0, results[0].boundingBox.height, BOUNDING_BOX_SIZE_ERROR);
        assertEquals(24.0, results[0].boundingBox.x, BOUNDING_BOX_POSITION_ERROR);
        assertEquals(20.0, results[0].boundingBox.y, BOUNDING_BOX_POSITION_ERROR);
    }

    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectValidImageWithAndroidAPI() {
        detectSucceedsOnValidImage(DetectionProviderType.ANDROID);
    }

    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectValidImageWithGmsCore() {
        if (TestUtils.IS_GMS_CORE_SUPPORTED) {
            detectSucceedsOnValidImage(DetectionProviderType.GMS_CORE);
        }
    }

    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectHandlesOddWidthWithAndroidAPI() throws Exception {
        // Pad the image so that the width is odd.
        Bitmap paddedBitmap = Bitmap.createBitmap(MONA_LISA_BITMAP.width + 1,
                MONA_LISA_BITMAP.height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(paddedBitmap);
        canvas.drawBitmap(BitmapUtils.convertToBitmap(MONA_LISA_BITMAP), 0, 0, null);
        org.chromium.skia.mojom.Bitmap mojoBitmap = TestUtils.mojoBitmapFromBitmap(paddedBitmap);
        assertEquals(1, mojoBitmap.width % 2);

        FaceDetectionResult[] results = detect(mojoBitmap, DetectionProviderType.ANDROID);
        assertEquals(1, results.length);
        assertEquals(40.0, results[0].boundingBox.width, BOUNDING_BOX_SIZE_ERROR);
        assertEquals(40.0, results[0].boundingBox.height, BOUNDING_BOX_SIZE_ERROR);
        assertEquals(24.0, results[0].boundingBox.x, BOUNDING_BOX_POSITION_ERROR);
        assertEquals(20.0, results[0].boundingBox.y, BOUNDING_BOX_POSITION_ERROR);
    }
}
