// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.shape_detection.mojom.FaceDetection;
import org.chromium.shape_detection.mojom.FaceDetectionResult;
import org.chromium.shape_detection.mojom.FaceDetectorOptions;
import org.chromium.skia.mojom.ColorType;

import java.nio.ByteBuffer;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Test suite for FaceDetectionImpl.
 */
public class FaceDetectionImplTest extends InstrumentationTestCase {
    public static final org.chromium.skia.mojom.Bitmap MONA_LISA_BITMAP =
            mojoBitmapFromFile("mona_lisa.jpg");
    // Different versions of Android have different implementations of FaceDetector.findFaces(), so
    // we have to use a large error threshold.
    public static final double BOUNDING_BOX_POSITION_ERROR = 10.0;
    public static final double BOUNDING_BOX_SIZE_ERROR = 5.0;

    public FaceDetectionImplTest() {}

    private static org.chromium.skia.mojom.Bitmap mojoBitmapFromBitmap(Bitmap bitmap) {
        ByteBuffer buffer = ByteBuffer.allocate(bitmap.getByteCount());
        bitmap.copyPixelsToBuffer(buffer);

        org.chromium.skia.mojom.Bitmap mojoBitmap = new org.chromium.skia.mojom.Bitmap();
        mojoBitmap.width = bitmap.getWidth();
        mojoBitmap.height = bitmap.getHeight();
        mojoBitmap.pixelData = buffer.array();
        mojoBitmap.colorType = ColorType.RGBA_8888;
        return mojoBitmap;
    }

    private static org.chromium.skia.mojom.Bitmap mojoBitmapFromFile(String relPath) {
        String path = UrlUtils.getIsolatedTestFilePath("services/test/data/" + relPath);
        Bitmap bitmap = BitmapFactory.decodeFile(path);
        return mojoBitmapFromBitmap(bitmap);
    }

    private static FaceDetectionResult[] detect(org.chromium.skia.mojom.Bitmap mojoBitmap) {
        FaceDetectorOptions options = new FaceDetectorOptions();
        options.fastMode = false;
        options.maxDetectedFaces = 32;
        FaceDetection detector = new FaceDetectionImpl(options);

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

    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectSucceedsOnValidImage() {
        FaceDetectionResult[] results = detect(MONA_LISA_BITMAP);
        assertEquals(1, results.length);
        assertEquals(40.0, results[0].boundingBox.width, BOUNDING_BOX_SIZE_ERROR);
        assertEquals(40.0, results[0].boundingBox.height, BOUNDING_BOX_SIZE_ERROR);
        assertEquals(24.0, results[0].boundingBox.x, BOUNDING_BOX_POSITION_ERROR);
        assertEquals(20.0, results[0].boundingBox.y, BOUNDING_BOX_POSITION_ERROR);
    }
}
