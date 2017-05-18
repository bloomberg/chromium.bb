// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.PointF;
import android.media.FaceDetector;
import android.media.FaceDetector.Face;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.mojo.system.SharedBufferHandle;
import org.chromium.mojo.system.SharedBufferHandle.MapFlags;
import org.chromium.shape_detection.mojom.FaceDetection;
import org.chromium.shape_detection.mojom.FaceDetectionResult;
import org.chromium.shape_detection.mojom.FaceDetectorOptions;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Collection;

/**
 * Test suite for Java Face Detection.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(sdk = 21, manifest = Config.NONE)
public class FaceDetectionImplTest {
    private static final int VALID_WIDTH = 1;
    private static final int VALID_HEIGHT = 1;
    private static final int INVALID_WIDTH = 0;
    private static final long NUM_BYTES = VALID_WIDTH * VALID_HEIGHT * 4;

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    // Class under test.
    private final FaceDetectionImpl mFaceDetectionImpl;

    public FaceDetectionImplTest(boolean fastMode) {
        FaceDetectorOptions options = new FaceDetectorOptions();
        options.fastMode = fastMode;
        options.maxDetectedFaces = 1;
        mFaceDetectionImpl = new FaceDetectionImpl(options);
    }

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
    }

    /**
     * Verify construction works.
     */
    @Test
    @Feature({"ShapeDetection"})
    public void testConstruction() {
        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());
    }

    /**
     * Verify detection fails if the SharedBufferHandle is invalid.
     */
    @Test
    @Feature({"ShapeDetection"})
    public void testDetectionFailsWithInvalidHandle() {
        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());

        SharedBufferHandle sharedBuffer = Mockito.mock(SharedBufferHandle.class);
        Mockito.when(sharedBuffer.isValid()).thenReturn(false);

        FaceDetection.DetectResponse callback = Mockito.mock(FaceDetection.DetectResponse.class);
        ArgumentCaptor<FaceDetectionResult[]> argument =
                ArgumentCaptor.forClass(FaceDetectionResult[].class);

        mFaceDetectionImpl.detect(sharedBuffer, VALID_WIDTH, VALID_HEIGHT, callback);

        Mockito.verify(sharedBuffer, Mockito.times(1)).isValid();
        Mockito.verify(callback, Mockito.times(1)).call(argument.capture());
        assertEquals("result", 0, argument.getValue().length);
    }

    /**
     * Verify detection fails if the sent dimensions are ugly.
     */
    @Test
    @Feature({"ShapeDetection"})
    public void testDetectionFailsWithInvalidDimensions() {
        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());

        SharedBufferHandle sharedBuffer = Mockito.mock(SharedBufferHandle.class);
        Mockito.when(sharedBuffer.isValid()).thenReturn(true);

        FaceDetection.DetectResponse callback = Mockito.mock(FaceDetection.DetectResponse.class);
        ArgumentCaptor<FaceDetectionResult[]> argument =
                ArgumentCaptor.forClass(FaceDetectionResult[].class);

        mFaceDetectionImpl.detect(sharedBuffer, INVALID_WIDTH, VALID_HEIGHT, callback);

        Mockito.verify(sharedBuffer, Mockito.times(1)).isValid();
        Mockito.verify(callback, Mockito.times(1)).call(argument.capture());
        assertEquals("result", 0, argument.getValue().length);
    }

    /**
     * Verify detection fails if SharedBufferHandle fails to map().
     */
    @Test
    @Feature({"ShapeDetection"})
    public void testDetectionFailsWithWronglyMappedBuffer() {
        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());

        SharedBufferHandle sharedBuffer = Mockito.mock(SharedBufferHandle.class);
        Mockito.when(sharedBuffer.isValid()).thenReturn(true);
        Mockito.when(sharedBuffer.map(
                             Mockito.eq(0L), Mockito.eq(NUM_BYTES), Mockito.any(MapFlags.class)))
                .thenReturn(ByteBuffer.allocate(0));

        FaceDetection.DetectResponse callback = Mockito.mock(FaceDetection.DetectResponse.class);
        ArgumentCaptor<FaceDetectionResult[]> argument =
                ArgumentCaptor.forClass(FaceDetectionResult[].class);

        mFaceDetectionImpl.detect(sharedBuffer, VALID_WIDTH, VALID_HEIGHT, callback);

        Mockito.verify(sharedBuffer, Mockito.times(1)).isValid();
        Mockito.verify(sharedBuffer, Mockito.times(1))
                .map(Mockito.eq(0L), Mockito.eq(NUM_BYTES), Mockito.any(MapFlags.class));
        Mockito.verify(callback, Mockito.times(1)).call(argument.capture());
        assertEquals("result", 0, argument.getValue().length);
    }

    /**
     * Verify detection works.
     */
    @Test
    @Feature({"ShapeDetection"})
    public void testDetection() {
        assertTrue("Should be on UI thread", ThreadUtils.runningOnUiThread());

        SharedBufferHandle sharedBuffer = Mockito.mock(SharedBufferHandle.class);
        Mockito.when(sharedBuffer.isValid()).thenReturn(true);
        Mockito.when(sharedBuffer.map(
                             Mockito.eq(0L), Mockito.eq(NUM_BYTES), Mockito.any(MapFlags.class)))
                .thenReturn(ByteBuffer.allocate((int) NUM_BYTES));

        FaceDetection.DetectResponse callback = Mockito.mock(FaceDetection.DetectResponse.class);
        ArgumentCaptor<FaceDetectionResult[]> argument =
                ArgumentCaptor.forClass(FaceDetectionResult[].class);

        // We have to add a |faceDetector| mock that inside returns a |face| mock. To spice things
        // up, some methods, findFaces() and getMidPoint(), modify parameters so we need doAnswer().
        final float eyesDistance = 1.0f;
        final float faceCenterX = 2.0f;
        final float faceCenterY = 3.0f;

        Face face = Mockito.mock(Face.class);
        Mockito.when(face.eyesDistance()).thenReturn(eyesDistance);
        Mockito.doAnswer(new Answer<Void>() {
                @Override
                public Void answer(InvocationOnMock invocation) {
                    Object[] args = invocation.getArguments();
                    PointF midPoint = (PointF) args[0];
                    midPoint.set(faceCenterX, faceCenterY);
                    return null;
                }
            })
                .when(face)
                .getMidPoint(Mockito.any(PointF.class));

        FaceDetector faceDetector = Mockito.mock(FaceDetector.class);
        FaceDetectionImpl.setFaceDetector(faceDetector);
        Mockito.doAnswer(new Answer<Integer>() {
                @Override
                public Integer answer(InvocationOnMock invocation) throws Throwable {
                    Object[] args = invocation.getArguments();
                    Face[] faceArray = (Face[]) args[1];
                    faceArray[0] = face;
                    return 1;
                }
            })
                .when(faceDetector)
                .findFaces(Mockito.any(Bitmap.class), Mockito.any());

        mFaceDetectionImpl.detect(sharedBuffer, VALID_WIDTH, VALID_HEIGHT, callback);

        Mockito.verify(sharedBuffer, Mockito.times(1)).isValid();
        Mockito.verify(sharedBuffer, Mockito.times(1))
                .map(Mockito.anyLong(), Mockito.anyLong(), Mockito.any(MapFlags.class));

        Mockito.verify(faceDetector, Mockito.times(1))
                .findFaces(Mockito.any(Bitmap.class), Mockito.any());

        // Detection happens asynchronously: give some timeout.
        Mockito.verify(callback, Mockito.timeout(3000 /* milliseconds */).times(1))
                .call(argument.capture());
        assertEquals("result", 1, argument.getValue().length);
        assertEquals("width", 2 * eyesDistance, argument.getValue()[0].boundingBox.width, 0.1);
        assertEquals("height", 2 * eyesDistance, argument.getValue()[0].boundingBox.height, 0.1);
        assertEquals("x", faceCenterX - eyesDistance, argument.getValue()[0].boundingBox.x, 0.1);
        assertEquals("y", faceCenterY - eyesDistance, argument.getValue()[0].boundingBox.y, 0.1);
    }
}
