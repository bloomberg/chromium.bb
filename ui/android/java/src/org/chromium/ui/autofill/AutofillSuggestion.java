// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.autofill;

import org.chromium.ui.DropdownItem;

/**
 * Autofill suggestion container used to store information needed for each Autofill popup entry.
 */
public class AutofillSuggestion implements DropdownItem {
    private final String mLabel;
    private final String mSublabel;
    private final int mIconId;
    private final int mSuggestionId;
    private final boolean mDeletable;

    /**
     * Constructs a Autofill suggestion container.
     * @param label The main label of the Autofill suggestion.
     * @param sublabel The describing sublabel of the Autofill suggestion.
     * @param suggestionId The type of suggestion.
     * @param deletable Whether the item can be deleted by the user.
     */
    public AutofillSuggestion(
            String label, String sublabel, int iconId, int suggestionId, boolean deletable) {
        mLabel = label;
        mSublabel = sublabel;
        mIconId = iconId;
        mSuggestionId = suggestionId;
        mDeletable = deletable;
    }

    @Override
    public String getLabel() {
        return mLabel;
    }

    @Override
    public String getSublabel() {
        return mSublabel;
    }

    @Override
    public int getIconId() {
        return mIconId;
    }

    @Override
    public boolean isEnabled() {
        return true;
    }

    @Override
    public boolean isGroupHeader() {
        return false;
    }

    public int getSuggestionId() {
        return mSuggestionId;
    }

    public boolean isDeletable() {
        return mDeletable;
    }

    public boolean isFillable() {
        // Negative suggestion ID indiciates a tool like "settings" or "scan credit card."
        // Non-negative suggestion ID indicates suggestions that can be filled into the form.
        return mSuggestionId >= 0;
    }
}
