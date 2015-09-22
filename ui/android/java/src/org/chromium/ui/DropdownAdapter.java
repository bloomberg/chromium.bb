// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView.LayoutParams;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;

import java.util.List;
import java.util.Set;

/**
 * Dropdown item adapter for DropdownPopupWindow.
 */
public class DropdownAdapter extends ArrayAdapter<DropdownItem> {
    private final Context mContext;
    private final Set<Integer> mSeparators;
    private final boolean mAreAllItemsEnabled;

    public DropdownAdapter(
            Context context, List<? extends DropdownItem> items, Set<Integer> separators) {
        super(context, R.layout.dropdown_item);
        addAll(items);
        mSeparators = separators;
        mContext = context;
        mAreAllItemsEnabled = checkAreAllItemsEnabled();
    }

    private boolean checkAreAllItemsEnabled() {
        for (int i = 0; i < getCount(); i++) {
            DropdownItem item = getItem(i);
            if (item.isEnabled() && !item.isGroupHeader()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View layout = convertView;
        if (convertView == null) {
            LayoutInflater inflater =
                    (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            layout = inflater.inflate(R.layout.dropdown_item, null);
            layout.setBackground(new DropdownDividerDrawable());
        }

        DropdownDividerDrawable divider = (DropdownDividerDrawable) layout.getBackground();
        int height = mContext.getResources().getDimensionPixelSize(R.dimen.dropdown_item_height);
        if (position == 0) {
            divider.setColor(Color.TRANSPARENT);
        } else {
            int dividerHeight = mContext.getResources().getDimensionPixelSize(
                    R.dimen.dropdown_item_divider_height);
            height += dividerHeight;
            divider.setHeight(dividerHeight);
            if (mSeparators != null && mSeparators.contains(position)) {
                divider.setColor(ApiCompatibilityUtils.getColor(mContext.getResources(),
                        R.color.dropdown_dark_divider_color));
            } else {
                divider.setColor(ApiCompatibilityUtils.getColor(mContext.getResources(),
                        R.color.dropdown_divider_color));
            }
        }

        // Note: trying to set the height of the root LinearLayout breaks accessibility,
        // so we have to adjust the height of this LinearLayout that wraps the TextViews instead.
        // If you need to modify this layout, don't forget to test it with TalkBack and make sure
        // it doesn't regress.
        // http://crbug.com/429364
        View wrapper = layout.findViewById(R.id.dropdown_label_wrapper);
        wrapper.setLayoutParams(
                new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, height, 1));

        DropdownItem item = getItem(position);
        TextView labelView = (TextView) layout.findViewById(R.id.dropdown_label);
        labelView.setText(item.getLabel());

        labelView.setEnabled(item.isEnabled());
        if (item.isGroupHeader()) {
            labelView.setTypeface(null, Typeface.BOLD);
        } else {
            labelView.setTypeface(null, Typeface.NORMAL);
        }

        TextView sublabelView = (TextView) layout.findViewById(R.id.dropdown_sublabel);
        CharSequence sublabel = item.getSublabel();
        if (TextUtils.isEmpty(sublabel)) {
            sublabelView.setVisibility(View.GONE);
        } else {
            sublabelView.setText(sublabel);
            sublabelView.setVisibility(View.VISIBLE);
        }

        ImageView iconView = (ImageView) layout.findViewById(R.id.dropdown_icon);
        if (item.getIconId() == DropdownItem.NO_ICON) {
            iconView.setVisibility(View.GONE);
        } else {
            iconView.setImageResource(item.getIconId());
            iconView.setVisibility(View.VISIBLE);
        }

        return layout;
    }

    @Override
    public boolean areAllItemsEnabled() {
        return mAreAllItemsEnabled;
    }

    @Override
    public boolean isEnabled(int position) {
        if (position < 0 || position >= getCount()) return false;
        DropdownItem item = getItem(position);
        return item.isEnabled() && !item.isGroupHeader();
    }
}
