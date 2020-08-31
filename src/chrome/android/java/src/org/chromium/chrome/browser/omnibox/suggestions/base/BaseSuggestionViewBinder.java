// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * Binds base suggestion view properties.
 *
 * This binder should be used by all suggestions that also utilize BaseSuggestionView<T> to
 * construct the view, and manages shared suggestion properties (such as decorations or theme).
 *
 * @param <T> The inner content view type being updated.
 */
public final class BaseSuggestionViewBinder<T extends View>
        implements ViewBinder<PropertyModel, BaseSuggestionView<T>, PropertyKey> {
    private final ViewBinder<PropertyModel, T, PropertyKey> mContentBinder;

    public BaseSuggestionViewBinder(ViewBinder<PropertyModel, T, PropertyKey> contentBinder) {
        mContentBinder = contentBinder;
    }

    @Override
    public void bind(PropertyModel model, BaseSuggestionView<T> view, PropertyKey propertyKey) {
        mContentBinder.bind(model, view.getContentView(), propertyKey);

        if (BaseSuggestionViewProperties.SUGGESTION_DELEGATE == propertyKey) {
            view.setDelegate(model.get(BaseSuggestionViewProperties.SUGGESTION_DELEGATE));
            updateContentViewPadding(model, view.getDecoratedSuggestionView());
        } else if (BaseSuggestionViewProperties.ICON == propertyKey) {
            updateSuggestionIcon(model, view);
            updateContentViewPadding(model, view.getDecoratedSuggestionView());
        } else if (BaseSuggestionViewProperties.ACTION_ICON == propertyKey) {
            updateActionIcon(model, view);
        } else if (BaseSuggestionViewProperties.ACTION_CALLBACK == propertyKey) {
            final Runnable callback = model.get(BaseSuggestionViewProperties.ACTION_CALLBACK);
            if (callback != null) {
                view.getActionImageView().setOnClickListener(v -> callback.run());
            } else {
                view.getActionImageView().setOnClickListener(null);
            }
        } else if (BaseSuggestionViewProperties.DENSITY == propertyKey) {
            updateContentViewPadding(model, view.getDecoratedSuggestionView());
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION == propertyKey) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
            updateContentViewPadding(model, view.getDecoratedSuggestionView());
        } else if (SuggestionCommonProperties.USE_DARK_COLORS == propertyKey) {
            updateSuggestionIcon(model, view);
            updateActionIcon(model, view);
        }
    }

    /** Returns which color scheme should be used to tint drawables. */
    private static boolean isDarkMode(PropertyModel model) {
        return model.get(SuggestionCommonProperties.USE_DARK_COLORS);
    }

    /** Update attributes of decorated suggestion icon. */
    private static <T extends View> void updateSuggestionIcon(
            PropertyModel model, BaseSuggestionView<T> baseView) {
        final RoundedCornerImageView rciv = baseView.getSuggestionImageView();
        final SuggestionDrawableState sds = model.get(BaseSuggestionViewProperties.ICON);

        if (sds != null) {
            final Resources res = rciv.getContext().getResources();
            final int paddingStart = res.getDimensionPixelSize(sds.isLarge
                            ? R.dimen.omnibox_suggestion_36dp_icon_margin_start
                            : R.dimen.omnibox_suggestion_24dp_icon_margin_start);
            final int paddingEnd = res.getDimensionPixelSize(sds.isLarge
                            ? R.dimen.omnibox_suggestion_36dp_icon_margin_end
                            : R.dimen.omnibox_suggestion_24dp_icon_margin_end);
            final int edgeSize = res.getDimensionPixelSize(sds.isLarge
                            ? R.dimen.omnibox_suggestion_36dp_icon_size
                            : R.dimen.omnibox_suggestion_24dp_icon_size);

            rciv.setPadding(paddingStart, 0, paddingEnd, 0);
            rciv.setMinimumHeight(edgeSize);

            // TODO(ender): move logic applying corner rounding to updateIcon when action images use
            // RoundedCornerImageView too.
            int radius = sds.useRoundedCorners
                    ? res.getDimensionPixelSize(R.dimen.default_rounded_corner_radius)
                    : 0;
            rciv.setRoundedCorners(radius, radius, radius, radius);
        }

        updateIcon(rciv, sds, ChromeColors.getSecondaryIconTintRes(!isDarkMode(model)));
    }

    /** Update attributes of decorated suggestion icon. */
    private static <T extends View> void updateActionIcon(
            PropertyModel model, BaseSuggestionView<T> baseView) {
        final ImageView view = baseView.getActionImageView();
        final SuggestionDrawableState sds = model.get(BaseSuggestionViewProperties.ACTION_ICON);
        updateIcon(view, sds, ChromeColors.getPrimaryIconTintRes(!isDarkMode(model)));
    }

    /**
     * Update content view padding.
     * This is required only to adjust the leading padding for undecorated suggestions.
     * TODO(crbug.com/1019937): remove after suggestion favicons are launched.
     */
    private static <T extends View> void updateContentViewPadding(
            PropertyModel model, DecoratedSuggestionView<T> view) {
        final SuggestionDrawableState sds = model.get(BaseSuggestionViewProperties.ICON);
        final int startSpace = sds == null ? view.getResources().getDimensionPixelSize(
                                       R.dimen.omnibox_suggestion_start_offset_without_icon)
                                           : 0;

        // TODO(ender): Drop this view and expand the last icon size by 8dp to ensure it remains
        // centered with the omnibox "Clear" button.
        final int endSpace = view.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_view_modern_end_padding);
        view.setPaddingRelative(startSpace, 0, endSpace, 0);

        // Compact suggestion handling: apply additional padding to the suggestion content.
        final @BaseSuggestionViewProperties.Density int density =
                model.get(BaseSuggestionViewProperties.DENSITY);

        int minimumHeightRes;
        int verticalPadRes;
        switch (density) {
            case BaseSuggestionViewProperties.Density.COMPACT:
                verticalPadRes = R.dimen.omnibox_suggestion_compact_padding;
                minimumHeightRes = R.dimen.omnibox_suggestion_compact_height;
                break;
            case BaseSuggestionViewProperties.Density.SEMICOMPACT:
                verticalPadRes = R.dimen.omnibox_suggestion_semicompact_padding;
                minimumHeightRes = R.dimen.omnibox_suggestion_semicompact_height;
                break;
            case BaseSuggestionViewProperties.Density.COMFORTABLE:
            default:
                verticalPadRes = R.dimen.omnibox_suggestion_comfortable_padding;
                minimumHeightRes = R.dimen.omnibox_suggestion_comfortable_height;
                break;
        }
        final int verticalPad = view.getResources().getDimensionPixelSize(verticalPadRes);
        view.getContentView().setPaddingRelative(0, verticalPad, 0, verticalPad);

        final int minimumHeight = view.getResources().getDimensionPixelSize(minimumHeightRes);
        view.getContentView().setMinimumHeight(minimumHeight);
    }

    /** Update image view using supplied drawable state object. */
    private static void updateIcon(
            ImageView view, SuggestionDrawableState sds, @ColorRes int tintRes) {
        final Resources res = view.getContext().getResources();

        view.setVisibility(sds == null ? View.GONE : View.VISIBLE);
        if (sds == null) {
            // Release any drawable that is still attached to this view to reclaim memory.
            view.setImageDrawable(null);
            return;
        }

        ColorStateList tint = null;
        if (sds.allowTint) {
            tint = AppCompatResources.getColorStateList(view.getContext(), tintRes);
        }

        view.setImageDrawable(sds.drawable);
        ApiCompatibilityUtils.setImageTintList(view, tint);
    }
}
