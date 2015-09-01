// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

/**
 * Utility class for Cardboard activity.
 */
public class CardboardActivityUtility {
    private static final int BYTE_PER_FLOAT = 4;

    /**
     * Create rectangular texture float buffer.
     */
    public static FloatBuffer makeRectangularTextureBuffer(float left, float right,
            float bottom, float top) {
        float[] position = new float[] {
            left, bottom,
            left, top,
            right, bottom,
            left, top,
            right, top,
            right, bottom
        };
        return makeFloatBuffer(position);
    }

    /**
     * Convert float array to a FloatBuffer for use in OpenGL calls.
     */
    public static FloatBuffer makeFloatBuffer(float[] data) {
        FloatBuffer result = ByteBuffer
                .allocateDirect(data.length * BYTE_PER_FLOAT)
                .order(ByteOrder.nativeOrder()).asFloatBuffer();
        result.put(data).position(0);
        return result;
    }
}