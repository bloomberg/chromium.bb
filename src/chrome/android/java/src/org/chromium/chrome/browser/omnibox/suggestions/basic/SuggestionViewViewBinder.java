// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.v4.view.ViewCompat;
import android.support.v7.content.res.AppCompatResources;
import android.text.Spannable;
import android.text.TextUtils;
import android.util.Pair;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionView.SuggestionIconType;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionIcon;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the basic suggestion view. */
public class SuggestionViewViewBinder {
    /**
     * @see
     * PropertyModelChangeProcessor.ViewBinder#bind(Object,
     * Object, Object)
     */
    public static void bind(PropertyModel model, SuggestionView view, PropertyKey propertyKey) {
        if (SuggestionViewProperties.DELEGATE.equals(propertyKey)) {
            view.setDelegate(model.get(SuggestionViewProperties.DELEGATE));
        } else if (SuggestionCommonProperties.USE_DARK_COLORS.equals(propertyKey)) {
            boolean useDarkColors = model.get(SuggestionCommonProperties.USE_DARK_COLORS);
            view.updateRefineIconTint(useDarkColors);
            view.updateSuggestionIconTint(useDarkColors);
            view.getTextLine1().setTextColor(
                    ApiCompatibilityUtils.getColor(view.getContext().getResources(),
                            useDarkColors ? R.color.default_text_color_dark
                                          : R.color.default_text_color_light));
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION.equals(propertyKey)) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (SuggestionViewProperties.IS_ANSWER.equals(propertyKey)) {
            updateSuggestionLayoutType(view, model);
        } else if (SuggestionViewProperties.HAS_ANSWER_IMAGE.equals(propertyKey)) {
            int visibility =
                    model.get(SuggestionViewProperties.HAS_ANSWER_IMAGE) ? View.VISIBLE : View.GONE;
            view.getAnswerImageView().setVisibility(visibility);
        } else if (SuggestionViewProperties.ANSWER_IMAGE.equals(propertyKey)) {
            view.getAnswerImageView().setImageBitmap(
                    model.get(SuggestionViewProperties.ANSWER_IMAGE));
        } else if (SuggestionViewProperties.REFINABLE.equals(propertyKey)) {
            boolean refinable = model.get(SuggestionViewProperties.REFINABLE);
            view.setRefinable(refinable);
            if (refinable) {
                view.initRefineIcon(model.get(SuggestionCommonProperties.USE_DARK_COLORS));
            }
        } else if (SuggestionViewProperties.SUGGESTION_ICON_TYPE.equals(propertyKey)
                || SuggestionViewProperties.SUGGESTION_ICON_BITMAP.equals(propertyKey)) {
            updateSuggestionIcon(view, model);
        } else if (SuggestionViewProperties.TEXT_LINE_1_SIZING.equals(propertyKey)) {
            Pair<Integer, Integer> sizing = model.get(SuggestionViewProperties.TEXT_LINE_1_SIZING);
            view.getTextLine1().setTextSize(sizing.first, sizing.second);
        } else if (SuggestionViewProperties.TEXT_LINE_1_MAX_LINES.equals(propertyKey)) {
            updateSuggestionLayoutType(view, model);
            updateSuggestionViewTextMaxLines(
                    view.getTextLine1(), model.get(SuggestionViewProperties.TEXT_LINE_1_MAX_LINES));
        } else if (SuggestionViewProperties.TEXT_LINE_1_TEXT_COLOR.equals(propertyKey)) {
            view.getTextLine1().setTextColor(
                    model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT_COLOR));
        } else if (SuggestionViewProperties.TEXT_LINE_1_TEXT_DIRECTION.equals(propertyKey)) {
            view.getTextLine1().setTextDirection(
                    model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT_DIRECTION));
        } else if (SuggestionViewProperties.TEXT_LINE_1_TEXT.equals(propertyKey)) {
            view.getTextLine1().setText(model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT).text);
        } else if (SuggestionViewProperties.TEXT_LINE_2_SIZING.equals(propertyKey)) {
            Pair<Integer, Integer> sizing = model.get(SuggestionViewProperties.TEXT_LINE_2_SIZING);
            view.getTextLine2().setTextSize(sizing.first, sizing.second);
        } else if (SuggestionViewProperties.TEXT_LINE_2_MAX_LINES.equals(propertyKey)) {
            updateSuggestionLayoutType(view, model);
            updateSuggestionViewTextMaxLines(
                    view.getTextLine2(), model.get(SuggestionViewProperties.TEXT_LINE_2_MAX_LINES));
        } else if (SuggestionViewProperties.TEXT_LINE_2_TEXT_COLOR.equals(propertyKey)) {
            view.getTextLine2().setTextColor(
                    model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT_COLOR));
        } else if (SuggestionViewProperties.TEXT_LINE_2_TEXT_DIRECTION.equals(propertyKey)) {
            view.getTextLine2().setTextDirection(
                    model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT_DIRECTION));
        } else if (SuggestionCommonProperties.SHOW_SUGGESTION_ICONS.equals(propertyKey)) {
            boolean showIcons = model.get(SuggestionCommonProperties.SHOW_SUGGESTION_ICONS);
            view.setSuggestionIconAreaWidthRes(showIcons
                            ? R.dimen.omnibox_suggestion_start_offset_with_icon
                            : R.dimen.omnibox_suggestion_start_offset_without_icon);
        } else if (SuggestionViewProperties.TEXT_LINE_2_TEXT.equals(propertyKey)) {
            Spannable line2Text = model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT).text;
            if (TextUtils.isEmpty(line2Text)) {
                view.getTextLine2().setVisibility(View.INVISIBLE);
            } else {
                view.getTextLine2().setVisibility(View.VISIBLE);
                view.getTextLine2().setText(line2Text);
            }
        }
    }

    /**
     * Adjust properties of the text field to properly display single- and multi-line answers.
     */
    private static void updateSuggestionViewTextMaxLines(TextView view, int lines) {
        if (lines == 1) {
            view.setEllipsize(null);
            view.setSingleLine();
        } else {
            view.setSingleLine(false);
            view.setEllipsize(TextUtils.TruncateAt.END);
            view.setMaxLines(lines);
        }
    }

    private static void updateSuggestionLayoutType(SuggestionView view, PropertyModel model) {
        boolean isAnswer = model.get(SuggestionViewProperties.IS_ANSWER);
        // Note: only one of these fields will report line count > 0; this depends on the selected
        // suggestion layout. Old layout allows multiline response in line 2, new answer layout - in
        // line 1.
        boolean isMultiline = model.get(SuggestionViewProperties.TEXT_LINE_2_MAX_LINES) > 1
                || model.get(SuggestionViewProperties.TEXT_LINE_1_MAX_LINES) > 1;
        if (!isAnswer) {
            view.setSuggestionLayoutType(SuggestionView.SuggestionLayoutType.TEXT_SUGGESTION);
        } else {
            view.setSuggestionLayoutType(isMultiline
                            ? SuggestionView.SuggestionLayoutType.MULTI_LINE_ANSWER
                            : SuggestionView.SuggestionLayoutType.ANSWER);
        }
    }

    private static void updateSuggestionIcon(SuggestionView view, PropertyModel model) {
        if (!model.get(SuggestionCommonProperties.SHOW_SUGGESTION_ICONS)) return;

        Drawable icon = null;
        boolean allowTint = true;
        @SuggestionIconType
        int iconType = SuggestionIconType.FALLBACK;

        Bitmap iconBitmap = model.get(SuggestionViewProperties.SUGGESTION_ICON_BITMAP);
        if (iconBitmap != null) {
            icon = new BitmapDrawable(iconBitmap);
            allowTint = false;
            iconType = SuggestionIconType.FAVICON;
        } else {
            @SuggestionIcon
            int type = model.get(SuggestionViewProperties.SUGGESTION_ICON_TYPE);

            int drawableId = R.drawable.ic_omnibox_page;
            switch (type) {
                case SuggestionIcon.GLOBE:
                    drawableId = R.drawable.ic_globe_24dp;
                    break;
                case SuggestionIcon.BOOKMARK:
                    drawableId = R.drawable.btn_star;
                    break;
                case SuggestionIcon.MAGNIFIER:
                    drawableId = R.drawable.ic_suggestion_magnifier;
                    break;
                case SuggestionIcon.HISTORY:
                    drawableId = R.drawable.ic_suggestion_history;
                    break;
                case SuggestionIcon.VOICE:
                    drawableId = R.drawable.btn_mic;
                    break;
                case SuggestionIcon.CALCULATOR:
                    drawableId = R.drawable.ic_equals_sign_round;
                    allowTint = false;
                    break;
                case SuggestionIcon.UNDEFINED:
                    // Since SuggestionViews may be re-used, there is a risk we would be
                    // presenting an stale favicon already.
                    assert false : "Unknown suggestion icon type.";
                    break;
                default:
                    break;
            }
            icon = AppCompatResources.getDrawable(view.getContext(), drawableId);
        }

        view.setSuggestionIconDrawable(
                icon, iconType, allowTint, model.get(SuggestionCommonProperties.USE_DARK_COLORS));
    }
}
