// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.os.Build;
import android.text.Editable;
import android.text.Selection;
import android.text.TextUtils;
import android.view.ActionMode;

import androidx.annotation.ColorInt;
import androidx.annotation.RequiresApi;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.AutocompleteText;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.UrlBarTextState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxTheme;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Handles translating the UrlBar model data to the view state.
 */
class UrlBarViewBinder {
    /**
     * @see
     * PropertyModelChangeProcessor.ViewBinder#bind(Object,
     * Object, Object)
     */
    public static void bind(PropertyModel model, UrlBar view, PropertyKey propertyKey) {
        if (UrlBarProperties.ACTION_MODE_CALLBACK.equals(propertyKey)) {
            ActionMode.Callback callback = model.get(UrlBarProperties.ACTION_MODE_CALLBACK);
            if (callback == null && view.getCustomSelectionActionModeCallback() == null) return;
            view.setCustomSelectionActionModeCallback(callback);
        } else if (UrlBarProperties.ALLOW_FOCUS.equals(propertyKey)) {
            view.setAllowFocus(model.get(UrlBarProperties.ALLOW_FOCUS));
        } else if (UrlBarProperties.AUTOCOMPLETE_TEXT.equals(propertyKey)) {
            AutocompleteText autocomplete = model.get(UrlBarProperties.AUTOCOMPLETE_TEXT);
            if (view.shouldAutocomplete()) {
                view.setAutocompleteText(autocomplete.userText, autocomplete.autocompleteText);
            }
        } else if (UrlBarProperties.DELEGATE.equals(propertyKey)) {
            view.setDelegate(model.get(UrlBarProperties.DELEGATE));
        } else if (UrlBarProperties.FOCUS_CHANGE_CALLBACK.equals(propertyKey)) {
            final Callback<Boolean> focusChangeCallback =
                    model.get(UrlBarProperties.FOCUS_CHANGE_CALLBACK);
            view.setOnFocusChangeListener((v, focused) -> {
                if (focused) view.setIgnoreTextChangesForAutocomplete(false);
                focusChangeCallback.onResult(focused);
            });
        } else if (UrlBarProperties.SHOW_CURSOR.equals(propertyKey)) {
            view.setCursorVisible(model.get(UrlBarProperties.SHOW_CURSOR));
        } else if (UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE.equals(propertyKey)) {
            view.setTextContextMenuDelegate(model.get(UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE));
        } else if (UrlBarProperties.TEXT_STATE.equals(propertyKey)) {
            UrlBarTextState state = model.get(UrlBarProperties.TEXT_STATE);
            view.setIgnoreTextChangesForAutocomplete(true);
            view.setText(state.text);
            view.setTextForAutofillServices(state.textForAutofillServices);
            view.setScrollState(state.scrollType, state.scrollToIndex);
            view.setIgnoreTextChangesForAutocomplete(false);

            if (view.hasFocus()) {
                if (state.selectionState == UrlBarCoordinator.SelectionState.SELECT_ALL) {
                    view.selectAll();
                } else if (state.selectionState == UrlBarCoordinator.SelectionState.SELECT_END) {
                    view.setSelection(view.getText().length());
                }
            }
        } else if (UrlBarProperties.OMNIBOX_THEME.equals(propertyKey)) {
            updateTextColors(view, model.get(UrlBarProperties.OMNIBOX_THEME));
        } else if (UrlBarProperties.INCOGNITO_COLORS_ENABLED.equals(propertyKey)) {
            final boolean incognitoColorsEnabled =
                    model.get(UrlBarProperties.INCOGNITO_COLORS_ENABLED);
            updateHighlightColor(view, incognitoColorsEnabled);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                updateCursorAndSelectHandleColor(view, incognitoColorsEnabled);
            }
        } else if (UrlBarProperties.URL_DIRECTION_LISTENER.equals(propertyKey)) {
            view.setUrlDirectionListener(model.get(UrlBarProperties.URL_DIRECTION_LISTENER));
        } else if (UrlBarProperties.URL_TEXT_CHANGE_LISTENER.equals(propertyKey)) {
            view.setUrlTextChangeListener(model.get(UrlBarProperties.URL_TEXT_CHANGE_LISTENER));
        } else if (UrlBarProperties.TEXT_CHANGED_LISTENER.equals(propertyKey)) {
            view.setTextChangedListener(model.get(UrlBarProperties.TEXT_CHANGED_LISTENER));
        } else if (UrlBarProperties.WINDOW_DELEGATE.equals(propertyKey)) {
            view.setWindowDelegate(model.get(UrlBarProperties.WINDOW_DELEGATE));
        }
    }

    private static void updateTextColors(UrlBar view, @OmniboxTheme int omniboxTheme) {
        final @ColorInt int textColor =
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(view.getContext(), omniboxTheme);

        final @ColorInt int hintColor =
                OmniboxResourceProvider.getUrlBarHintTextColor(view.getContext(), omniboxTheme);

        view.setTextColor(textColor);
        setHintTextColor(view, hintColor);
    }

    private static void updateHighlightColor(UrlBar view, boolean useIncognitoColors) {
        @ColorInt
        int originalHighlightColor;
        Object highlightColorObj = view.getTag(R.id.highlight_color);
        if (highlightColorObj == null || !(highlightColorObj instanceof Integer)) {
            originalHighlightColor = view.getHighlightColor();
            view.setTag(R.id.highlight_color, originalHighlightColor);
        } else {
            originalHighlightColor = (Integer) highlightColorObj;
        }

        int highlightColor;
        if (useIncognitoColors) {
            highlightColor = view.getResources().getColor(R.color.text_highlight_color_incognito);
        } else {
            highlightColor = originalHighlightColor;
        }

        view.setHighlightColor(highlightColor);
    }

    @RequiresApi(api = Build.VERSION_CODES.Q)
    private static void updateCursorAndSelectHandleColor(UrlBar view, boolean useIncognitoColors) {
        final int color = useIncognitoColors
                ? view.getContext().getColor(R.color.default_control_color_active_dark)
                : MaterialColors.getColor(view, R.attr.colorPrimary);
        view.getTextCursorDrawable().mutate().setTint(color);
        view.getTextSelectHandle().mutate().setTint(color);
        view.getTextSelectHandleLeft().mutate().setTint(color);
        view.getTextSelectHandleRight().mutate().setTint(color);
    }

    private static void setHintTextColor(UrlBar view, @ColorInt int textColor) {
        // Note: Setting the hint text color only takes effect if there is no text in the URL bar.
        //       To get around this, set the URL to empty before setting the hint color and revert
        //       back to the previous text after.
        Editable text = view.getText();
        if (TextUtils.isEmpty(text)) {
            view.setHintTextColor(textColor);
            return;
        }

        int selectionStart = view.getSelectionStart();
        int selectionEnd = view.getSelectionEnd();

        // Make sure the setText in this block does not affect the suggestions.
        view.setIgnoreTextChangesForAutocomplete(true);
        view.setText("");
        view.setHintTextColor(textColor);
        view.setText(text);

        // Restore the previous selection, if there was one.
        if (selectionStart >= 0 && selectionEnd >= 0 && view.hasFocus()) {
            Selection.setSelection(view.getText(), selectionStart, selectionEnd);
        }

        view.setIgnoreTextChangesForAutocomplete(false);
    }

    private UrlBarViewBinder() {}
}
