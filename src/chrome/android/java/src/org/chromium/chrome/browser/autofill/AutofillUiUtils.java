// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.support.v4.view.ViewCompat;
import android.support.v4.widget.TextViewCompat;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupWindow;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * Helper methods that can be used across multiple Autofill UIs.
 */
public class AutofillUiUtils {
    // 200ms is chosen small enough not to be detectable to human eye, but big
    // enough for to avoid any race conditions on modern machines.
    private static final int TOOLTIP_DEFERRED_PERIOD_MS = 200;

    /**
     * Show Tooltip UI.
     *
     * @param context Context required to get resources.
     * @param popup {@PopupWindow} that shows tooltip UI.
     * @param text  Text to be shown in tool tip UI.
     * @param width Width of the textview.
     * @param anchorView Anchor view under which tooltip popup has to be shown
     * @param dismissAction Tooltip dismissive action.
     */
    public static void showTooltip(Context context, PopupWindow popup, int text, int width,
            View anchorView, final Runnable dismissAction) {
        TextView textView = new TextView(context);
        textView.setText(text);
        textView.setWidth(width);
        TextViewCompat.setTextAppearance(textView, R.style.WhiteBody);
        Resources resources = context.getResources();
        int hPadding = resources.getDimensionPixelSize(
                R.dimen.autofill_card_unmask_tooltip_horizontal_padding);
        int vPadding = resources.getDimensionPixelSize(
                R.dimen.autofill_card_unmask_tooltip_vertical_padding);
        textView.setPadding(hPadding, vPadding, hPadding, vPadding);

        popup.setContentView(textView);
        popup.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        popup.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        popup.setOutsideTouchable(true);
        popup.setBackgroundDrawable(ApiCompatibilityUtils.getDrawable(
                resources, R.drawable.store_locally_tooltip_background));
        popup.setOnDismissListener(() -> {
            Handler h = new Handler();
            h.postDelayed(dismissAction, TOOLTIP_DEFERRED_PERIOD_MS);
        });
        popup.showAsDropDown(anchorView, ViewCompat.getPaddingStart(anchorView), 0);
        textView.announceForAccessibility(textView.getText());
    }
}
