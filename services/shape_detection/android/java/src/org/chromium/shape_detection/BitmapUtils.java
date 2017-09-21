// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Bitmap;

import com.google.android.gms.vision.Frame;

import org.chromium.skia.mojom.ColorType;

import java.nio.ByteBuffer;

/**
 * Utility class to convert a Bitmap to a GMS core YUV Frame.
 */
public class BitmapUtils {
    public static Bitmap convertToBitmap(org.chromium.skia.mojom.Bitmap bitmapData) {
        int width = bitmapData.width;
        int height = bitmapData.height;
        final long numPixels = (long) width * height;
        // TODO(mcasas): https://crbug.com/670028 homogeneize overflow checking.
        if (bitmapData.pixelData == null || width <= 0 || height <= 0
                || numPixels > (Long.MAX_VALUE / 4)) {
            return null;
        }

        if (bitmapData.colorType != ColorType.RGBA_8888
                && bitmapData.colorType != ColorType.BGRA_8888) {
            return null;
        }

        ByteBuffer imageBuffer = ByteBuffer.wrap(bitmapData.pixelData);
        if (imageBuffer.capacity() <= 0) {
            return null;
        }

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        bitmap.copyPixelsFromBuffer(imageBuffer);

        return bitmap;
    }

    public static Frame convertToFrame(org.chromium.skia.mojom.Bitmap bitmapData) {
        Bitmap bitmap = convertToBitmap(bitmapData);
        if (bitmap == null) {
            return null;
        }

        // This constructor implies a pixel format conversion to YUV.
        return new Frame.Builder().setBitmap(bitmap).build();
    }
}
