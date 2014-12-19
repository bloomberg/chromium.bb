// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.system;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.graphics.RectF;

import org.chromium.ui.R;
import org.chromium.ui.gfx.DeviceDisplayInfo;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.SystemUIResourceType;
import org.chromium.ui.resources.async.AsyncPreloadResourceLoader;
import org.chromium.ui.resources.statics.StaticResource;

/**
 * Handles loading system specific resources like overscroll and edge glows.
 */
public class SystemResourceLoader extends AsyncPreloadResourceLoader {
    private static final float SIN_PI_OVER_6 = 0.5f;
    private static final float COS_PI_OVER_6 = 0.866f;

    /**
     * Creates an instance of a {@link SystemResourceLoader}.
     * @param resourceType The resource type this loader is responsible for loading.
     * @param callback     The {@link ResourceLoaderCallback} to notify when a {@link Resource} is
     *                     done loading.
     * @param resources    A {@link Resources} instance to load assets from.
     */
    public SystemResourceLoader(int resourceType, ResourceLoaderCallback callback,
            final Context context) {
        super(resourceType, callback, new ResourceCreator() {
            @Override
            public Resource create(int resId) {
                return createResource(context, resId);
            }
        });
    }

    private static Resource createResource(Context context, int resId) {
        switch (resId) {
            case SystemUIResourceType.OVERSCROLL_EDGE:
                return StaticResource.create(Resources.getSystem(),
                        getResourceId("android:drawable/overscroll_edge"), 128, 12);
            case SystemUIResourceType.OVERSCROLL_GLOW:
                return StaticResource.create(Resources.getSystem(),
                        getResourceId("android:drawable/overscroll_glow"), 128, 64);
            case SystemUIResourceType.OVERSCROLL_GLOW_L:
                return createOverscrollGlowLBitmap(context);

            // TODO(jdduke): Make refresh resources be of type ANDROID_RESOURCE_TYPE_STATIC.
            case SystemUIResourceType.OVERSCROLL_REFRESH_IDLE:
                return StaticResource.create(context.getResources(), R.drawable.refresh_gray, 0, 0);
            case SystemUIResourceType.OVERSCROLL_REFRESH_ACTIVE:
                return StaticResource.create(context.getResources(), R.drawable.refresh_blue, 0, 0);

            default:
                assert false;
        }
        return null;
    }

    private static Resource createOverscrollGlowLBitmap(Context context) {
        DeviceDisplayInfo displayInfo = DeviceDisplayInfo.create(context);
        int screenWidth = displayInfo.getPhysicalDisplayWidth() != 0
                ? displayInfo.getPhysicalDisplayWidth() : displayInfo.getDisplayWidth();
        int screenHeight = displayInfo.getPhysicalDisplayHeight() != 0
                ? displayInfo.getPhysicalDisplayHeight() : displayInfo.getDisplayHeight();

        float arcWidth = Math.min(screenWidth, screenHeight) * 0.5f / SIN_PI_OVER_6;
        float y = COS_PI_OVER_6 * arcWidth;
        float height = arcWidth - y;

        float arcRectX = -arcWidth / 2.f;
        float arcRectY = -arcWidth - y;
        float arcRectWidth = arcWidth * 2.f;
        float arcRectHeight = arcWidth * 2.f;
        RectF arcRect = new RectF(
                arcRectX, arcRectY, arcRectX + arcRectWidth, arcRectY + arcRectHeight);

        Paint arcPaint = new Paint();
        arcPaint.setAntiAlias(true);
        arcPaint.setAlpha(0xBB);
        arcPaint.setStyle(Style.FILL);

        Bitmap bitmap = Bitmap.createBitmap((int) arcWidth, (int) height, Bitmap.Config.ALPHA_8);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawArc(arcRect, 45, 90, true, arcPaint);

        return new StaticResource(bitmap);
    }

    private static int getResourceId(String name) {
        Resources systemResources = Resources.getSystem();
        return systemResources.getIdentifier(name, null, null);
    }
}
