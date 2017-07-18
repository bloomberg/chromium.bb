// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.skia.mojom.ColorType;

import java.nio.ByteBuffer;

/**
 * Utility class for ShapeDetection instrumentation tests,
 * provides support for e.g. reading files and converting
 * Bitmaps to mojom.Bitmaps.
 */
public class TestUtils {
    public static final boolean IS_GMS_CORE_SUPPORTED = isGmsCoreSupported();

    private static boolean isGmsCoreSupported() {
        return GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(
                       ContextUtils.getApplicationContext())
                == ConnectionResult.SUCCESS;
    }

    public static org.chromium.skia.mojom.Bitmap mojoBitmapFromBitmap(Bitmap bitmap) {
        ByteBuffer buffer = ByteBuffer.allocate(bitmap.getByteCount());
        bitmap.copyPixelsToBuffer(buffer);

        org.chromium.skia.mojom.Bitmap mojoBitmap = new org.chromium.skia.mojom.Bitmap();
        mojoBitmap.width = bitmap.getWidth();
        mojoBitmap.height = bitmap.getHeight();
        mojoBitmap.pixelData = buffer.array();
        mojoBitmap.colorType = ColorType.RGBA_8888;
        return mojoBitmap;
    }

    public static org.chromium.skia.mojom.Bitmap mojoBitmapFromFile(String relPath) {
        String path = UrlUtils.getIsolatedTestFilePath("services/test/data/" + relPath);
        Bitmap bitmap = BitmapFactory.decodeFile(path);
        return mojoBitmapFromBitmap(bitmap);
    }
}
