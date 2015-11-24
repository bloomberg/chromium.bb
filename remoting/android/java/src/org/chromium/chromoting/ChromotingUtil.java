// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;
import android.view.Menu;

import org.chromium.base.ApiCompatibilityUtils;

/** Utility methods for chromoting code. */
public abstract class ChromotingUtil {
    private static final String TAG = "Chromoting";

    /**
     * Tints all icons of a toolbar menu so they have the same color as the 'back' navigation icon
     * and the three-dots overflow icon.
     * @param context Context for getting theme and resource information.
     * @param menu Menu with icons to be tinted.
     */
    public static void tintMenuIcons(Context context, Menu menu) {
        int color = getColorAttribute(context, R.attr.colorControlNormal);
        int items = menu.size();
        for (int i = 0; i < items; i++) {
            Drawable icon = menu.getItem(i).getIcon();
            if (icon != null) {
                icon.mutate().setColorFilter(color, PorterDuff.Mode.SRC_IN);
            }
        }
    }

    /**
     * Returns a color from a theme attribute.
     * @param context Context with resources to look up.
     * @param attribute Attribute such as R.attr.colorControlNormal.
     * @return Color value.
     * @throws Resources.NotFoundException
     */
    public static int getColorAttribute(Context context, int attribute) {
        TypedValue typedValue = new TypedValue();
        if (!context.getTheme().resolveAttribute(attribute, typedValue, true)) {
            throw new Resources.NotFoundException("Attribute not found.");
        }

        if (typedValue.resourceId != 0) {
            // Attribute is a resource.
            return ApiCompatibilityUtils.getColor(context.getResources(), typedValue.resourceId);
        } else if (typedValue.type >= TypedValue.TYPE_FIRST_COLOR_INT
                && typedValue.type <= TypedValue.TYPE_LAST_COLOR_INT) {
            // Attribute is a raw color value.
            return typedValue.data;
        } else {
            throw new Resources.NotFoundException("Attribute not a color.");
        }
    }
}
