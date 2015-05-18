// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.autofill;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.View;
import android.widget.AdapterView;
import android.widget.PopupWindow;

import org.chromium.ui.DropdownAdapter;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.R;
import org.chromium.ui.base.ViewAndroidDelegate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/**
 * The Autofill suggestion popup that lists relevant suggestions.
 */
public class AutofillPopup extends DropdownPopupWindow implements AdapterView.OnItemClickListener,
        AdapterView.OnItemLongClickListener, PopupWindow.OnDismissListener {

    /**
     * The constant used to specify a separator in a list of Autofill suggestions.
     * Has to be kept in sync with enum in WebAutofillClient.h
     */
    private static final int ITEM_ID_SEPARATOR_ENTRY = -3;

    private final Context mContext;
    private final AutofillPopupDelegate mAutofillCallback;
    private List<AutofillSuggestion> mSuggestions;


    /**
     * An interface to handle the touch interaction with an AutofillPopup object.
     */
    public interface AutofillPopupDelegate {
        /**
         * Informs the controller the AutofillPopup was hidden.
         */
        public void dismissed();

        /**
         * Handles the selection of an Autofill suggestion from an AutofillPopup.
         * @param listIndex The index of the selected Autofill suggestion.
         */
        public void suggestionSelected(int listIndex);

        /**
         * Initiates the deletion process for an item. (A confirm dialog should be shown.)
         * @param listIndex The index of the suggestion to delete.
         */
        public void deleteSuggestion(int listIndex);
    }

    /**
     * Creates an AutofillWindow with specified parameters.
     * @param context Application context.
     * @param viewAndroidDelegate View delegate used to add and remove views.
     * @param autofillCallback A object that handles the calls to the native AutofillPopupView.
     */
    public AutofillPopup(Context context, ViewAndroidDelegate viewAndroidDelegate,
            AutofillPopupDelegate autofillCallback) {
        super(context, viewAndroidDelegate);
        mContext = context;
        mAutofillCallback = autofillCallback;

        setOnItemClickListener(this);
        setOnDismissListener(this);
        disableHideOnOutsideTap();
        setContentDescriptionForAccessibility(
                mContext.getString(R.string.autofill_popup_content_description));
    }

    /**
     * Filters the Autofill suggestions to the ones that we support and shows the popup.
     * @param suggestions Autofill suggestion data.
     */
    @SuppressLint("InlinedApi")
    public void filterAndShow(AutofillSuggestion[] suggestions, boolean isRtl) {
        mSuggestions = new ArrayList<AutofillSuggestion>(Arrays.asList(suggestions));
        // Remove the AutofillSuggestions with IDs that are not supported by Android
        ArrayList<DropdownItem> cleanedData = new ArrayList<DropdownItem>();
        HashSet<Integer> separators = new HashSet<Integer>();
        for (int i = 0; i < suggestions.length; i++) {
            int itemId = suggestions[i].getSuggestionId();
            if (itemId == ITEM_ID_SEPARATOR_ENTRY) {
                separators.add(cleanedData.size());
            } else {
                cleanedData.add(suggestions[i]);
            }
        }

        setAdapter(new DropdownAdapter(mContext, cleanedData, separators));
        setRtl(isRtl);
        show();
        getListView().setOnItemLongClickListener(this);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        DropdownAdapter adapter = (DropdownAdapter) parent.getAdapter();
        int listIndex = mSuggestions.indexOf(adapter.getItem(position));
        assert listIndex > -1;
        mAutofillCallback.suggestionSelected(listIndex);
    }

    @Override
    public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
        DropdownAdapter adapter = (DropdownAdapter) parent.getAdapter();
        AutofillSuggestion suggestion = (AutofillSuggestion) adapter.getItem(position);
        if (!suggestion.isDeletable()) return false;

        int listIndex = mSuggestions.indexOf(suggestion);
        assert listIndex > -1;
        mAutofillCallback.deleteSuggestion(listIndex);
        return true;
    }

    @Override
    public void onDismiss() {
        mAutofillCallback.dismissed();
    }
}
