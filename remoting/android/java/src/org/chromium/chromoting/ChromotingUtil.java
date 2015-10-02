// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;
import android.view.Menu;

import org.chromium.base.ApiCompatibilityUtils;

/** Utility methods for chromoting code. */
public abstract class ChromotingUtil {
    /**
     * Tints all icons of a toolbar menu so they have the same color as the 'back' navigation icon
     * and the three-dots overflow icon.
     * @param context Context for getting theme and resource information.
     * @param menu Menu with icons to be tinted.
     */
    public static void tintMenuIcons(Context context, Menu menu) {
        TypedValue typedValue = new TypedValue();
        context.getTheme().resolveAttribute(R.attr.colorControlNormal, typedValue, true);
        int color = ApiCompatibilityUtils.getColor(context.getResources(), typedValue.resourceId);
        int items = menu.size();
        for (int i = 0; i < items; i++) {
            Drawable icon = menu.getItem(i).getIcon();
            if (icon != null) {
                icon.mutate().setColorFilter(color, PorterDuff.Mode.SRC_IN);
            }
        }
    }
}
