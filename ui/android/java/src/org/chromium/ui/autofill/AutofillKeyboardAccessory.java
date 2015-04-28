// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.autofill;

import android.annotation.SuppressLint;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.AdapterView;
import android.widget.ListAdapter;
import android.widget.ListView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.ui.DropdownAdapter;
import org.chromium.ui.R;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashSet;

/**
 * The Autofill suggestion view that lists relevant suggestions. It sits above the keyboard and
 * below the content area.
 */
public class AutofillKeyboardAccessory extends ListView implements AdapterView.OnItemClickListener {
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
    @SuppressFBWarnings("NP_NULL_ON_SOME_PATH_FROM_RETURN_VALUE")
    public AutofillKeyboardAccessory(
            WindowAndroid windowAndroid, AutofillKeyboardAccessoryDelegate autofillCallback) {
        super(windowAndroid.getActivity().get());
        assert autofillCallback != null;
        assert windowAndroid.getActivity().get() != null;
        mWindowAndroid = windowAndroid;
        mAutofillCallback = autofillCallback;

        setBackgroundResource(R.drawable.autofill_accessory_view_border);
        setOnItemClickListener(this);
        setContentDescription(windowAndroid.getActivity().get().getString(
                R.string.autofill_popup_content_description));
    }

    /**
     * Shows the given suggestions.
     * @param suggestions Autofill suggestion data.
     * @param isRtl Gives the layout direction for the <input> field.
     */
    @SuppressLint("InlinedApi")
    public void showWithSuggestions(AutofillSuggestion[] suggestions, boolean isRtl) {
        setAdapter(new DropdownAdapter(
                mWindowAndroid.getActivity().get(), suggestions, new HashSet<Integer>()));
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
                listItem.measure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                        MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
                height += listItem.getMeasuredHeight();
            }
        }

        setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, height));

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
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        mAutofillCallback.suggestionSelected(position);
    }
}
