// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.listmenu;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;

/**
 * Class responsible for binding the model of the ListMenuItem and the view.
 */
public class ListMenuItemViewBinder {
    public static void binder(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = view.findViewById(R.id.menu_item_text);
        ImageView startIcon = view.findViewById(R.id.menu_item_icon);
        ImageView endIcon = view.findViewById(R.id.menu_item_end_icon);
        if (propertyKey == ListMenuItemProperties.TITLE_ID) {
            textView.setText(model.get(ListMenuItemProperties.TITLE_ID));
        } else if (propertyKey == ListMenuItemProperties.START_ICON_ID
                || propertyKey == ListMenuItemProperties.END_ICON_ID) {
            int id = model.get((ReadableIntPropertyKey) propertyKey);
            Drawable drawable =
                    id == 0 ? null : AppCompatResources.getDrawable(view.getContext(), id);
            if (drawable != null) {
                if (propertyKey == ListMenuItemProperties.START_ICON_ID) {
                    // need more space between the start and the icon if icon is on the start.
                    startIcon.setImageDrawable(drawable);
                    textView.setPaddingRelative(
                            view.getResources().getDimensionPixelOffset(R.dimen.menu_padding_start),
                            textView.getPaddingTop(), textView.getPaddingEnd(),
                            textView.getPaddingBottom());
                    startIcon.setVisibility(View.VISIBLE);
                    endIcon.setVisibility(View.GONE);
                } else {
                    // Move to the end.
                    endIcon.setImageDrawable(drawable);
                    startIcon.setVisibility(View.GONE);
                    endIcon.setVisibility(View.VISIBLE);
                }
            }
        } else if (propertyKey == ListMenuItemProperties.TINT_COLOR_ID) {
            ApiCompatibilityUtils.setImageTintList(startIcon,
                    ContextCompat.getColorStateList(
                            view.getContext(), model.get(ListMenuItemProperties.TINT_COLOR_ID)));
            ApiCompatibilityUtils.setImageTintList(endIcon,
                    ContextCompat.getColorStateList(
                            view.getContext(), model.get(ListMenuItemProperties.TINT_COLOR_ID)));
        } else if (propertyKey == ListMenuItemProperties.ENABLED) {
            textView.setEnabled(model.get(ListMenuItemProperties.ENABLED));
            startIcon.setEnabled(model.get(ListMenuItemProperties.ENABLED));
            endIcon.setEnabled(model.get(ListMenuItemProperties.ENABLED));
        }
    }
}
