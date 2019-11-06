// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.skia.mojom.ColorType;
import org.chromium.skia.mojom.ImageInfo;

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
        mojoBitmap.imageInfo = new ImageInfo();
        mojoBitmap.imageInfo.width = bitmap.getWidth();
        mojoBitmap.imageInfo.height = bitmap.getHeight();
        mojoBitmap.imageInfo.colorType = ColorType.RGBA_8888;
        mojoBitmap.pixelData = new org.chromium.mojo_base.mojom.BigBuffer();
        mojoBitmap.pixelData.setBytes(buffer.array());
        return mojoBitmap;
    }

    public static org.chromium.skia.mojom.Bitmap mojoBitmapFromFile(String relPath) {
        String path = UrlUtils.getIsolatedTestFilePath("services/test/data/" + relPath);
        Bitmap bitmap = BitmapFactory.decodeFile(path);
        return mojoBitmapFromBitmap(bitmap);
    }

    public static org.chromium.skia.mojom.Bitmap mojoBitmapFromText(String[] texts) {
        final int x = 10;
        final int baseline = 100;

        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setTextSize(36.0f);
        paint.setTextAlign(Paint.Align.LEFT);

        Bitmap bitmap = Bitmap.createBitmap(1080, 480, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.WHITE);

        for (int i = 0; i < texts.length; i++) {
            canvas.drawText(texts[i], x, baseline * (i + 1), paint);
        }

        return mojoBitmapFromBitmap(bitmap);
    }
}
