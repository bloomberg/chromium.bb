// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.autofill;

import android.annotation.SuppressLint;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.AdapterView;
import android.widget.FrameLayout;
import android.widget.ListAdapter;
import android.widget.ListView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.ui.R;
import org.chromium.ui.base.WindowAndroid;

/**
 * The Autofill suggestion view that lists relevant suggestions. It sits above the keyboard and
 * below the content area.
 */
public class AutofillKeyboardAccessory extends ListView implements AdapterView.OnItemClickListener,
        WindowAndroid.KeyboardVisibilityListener {
    private final WindowAndroid mWindowAndroid;
    private final AutofillKeyboardAccessoryDelegate mAutofillCallback;

    /**
     * An interface to handle the touch interaction with an AutofillKeyboardAccessory object.
     */
    public interface AutofillKeyboardAccessoryDelegate {
        /**
         * Informs the controller the AutofillKeyboardAccessory was hidden.
         */
        public void dismissed();

        /**
         * Handles the selection of an Autofill suggestion from an AutofillKeyboardAccessory.
         * @param listIndex The index of the selected Autofill suggestion.
         */
        public void suggestionSelected(int listIndex);
    }

    /**
     * Creates an AutofillKeyboardAccessory with specified parameters.
     * @param windowAndroid The owning WindowAndroid.
     * @param autofillCallback A object that handles the calls to the native
     * AutofillKeyboardAccessoryView.
     */
    public AutofillKeyboardAccessory(
            WindowAndroid windowAndroid, AutofillKeyboardAccessoryDelegate autofillCallback) {
        super(windowAndroid.getActivity().get());
        assert autofillCallback != null;
        assert windowAndroid.getActivity().get() != null;
        mWindowAndroid = windowAndroid;
        mAutofillCallback = autofillCallback;

        mWindowAndroid.addKeyboardVisibilityListener(this);
        setOnItemClickListener(this);
        setContentDescription(getContext().getString(
                R.string.autofill_popup_content_description));
        setBackgroundColor(getResources().getColor(
                R.color.keyboard_accessory_suggestion_background_color));
    }

    /**
     * Shows the given suggestions.
     * @param suggestions Autofill suggestion data.
     * @param isRtl Gives the layout direction for the <input> field.
     */
    @SuppressLint("InlinedApi")
    public void showWithSuggestions(AutofillSuggestion[] suggestions, boolean isRtl) {
        setAdapter(new SuggestionAdapter(getContext(), suggestions));
        ApiCompatibilityUtils.setLayoutDirection(
                this, isRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);

        int height = ViewGroup.LayoutParams.WRAP_CONTENT;
        // Limit the visible number of suggestions (others are accessible via scrolling).
        final int suggestionLimit = 2;
        ListAdapter listAdapter = getAdapter();
        if (listAdapter.getCount() > suggestionLimit) {
            height = 0;
            for (int i = 0; i < suggestionLimit; i++) {
                View listItem = listAdapter.getView(i, null, this);
                height += listItem.getLayoutParams().height;
            }
        }

        setLayoutParams(new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                height));

        if (getParent() == null) {
            ViewGroup container = mWindowAndroid.getKeyboardAccessoryView();
            container.addView(this);
            container.setVisibility(View.VISIBLE);
            sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        }
    }

    /**
     * Called to hide the suggestion view.
     */
    public void dismiss() {
        ViewGroup container = mWindowAndroid.getKeyboardAccessoryView();
        container.removeView(this);
        container.setVisibility(View.GONE);
        mWindowAndroid.removeKeyboardVisibilityListener(this);
        ((View) container.getParent()).requestLayout();
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        mAutofillCallback.suggestionSelected(position);
    }

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        if (!isShowing) {
            dismiss();
            mAutofillCallback.dismissed();
        }
    }
}
