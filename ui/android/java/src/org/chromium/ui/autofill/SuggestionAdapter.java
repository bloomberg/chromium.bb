// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.autofill;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.R;

/**
 * Adapter for AutofillKeyboardAccessory suggestions.
 */
public class SuggestionAdapter extends ArrayAdapter<DropdownItem> {
    private Context mContext;

    public SuggestionAdapter(Context context, DropdownItem[] items) {
        super(context, R.layout.autofill_suggestion_item, items);
        mContext = context;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View layout = convertView;
        if (convertView == null) {
            layout = LayoutInflater.from(mContext).inflate(R.layout.autofill_suggestion_item,
                    parent, false);
        }

        DropdownItem item = getItem(position);
        TextView labelView = (TextView) layout;
        labelView.setText(item.getLabel());
        ApiCompatibilityUtils.setCompoundDrawablesRelativeWithIntrinsicBounds(labelView,
                item.getIconId() != DropdownItem.NO_ICON ? item.getIconId() : 0, 0, 0, 0);

        return layout;
    }
}
