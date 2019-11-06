// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabViewBinder.ElementViewHolder;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;
import org.chromium.ui.widget.ChipView;

class CreditCardAccessorySheetViewBinder {
    static ElementViewHolder create(ViewGroup parent, @AccessorySheetDataPiece.Type int viewType) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.TITLE:
                return new AccessorySheetTabViewBinder.TitleViewHolder(
                        parent, R.layout.keyboard_accessory_sheet_tab_title);
            case AccessorySheetDataPiece.Type.CREDIT_CARD_INFO:
                return new CreditCardInfoViewHolder(parent);
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
                return AccessorySheetTabViewBinder.create(parent, viewType);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    /**
     * View which represents a single credit card and its selectable fields.
     */
    static class CreditCardInfoViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.UserInfo, CreditCardAccessoryInfoView> {
        CreditCardInfoViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_credit_card_info);
        }

        @Override
        protected void bind(KeyboardAccessoryData.UserInfo info, CreditCardAccessoryInfoView view) {
            bindChipView(view.getCCNumber(), info.getFields().get(0));
            bindChipView(view.getExpMonth(), info.getFields().get(1));
            bindChipView(view.getExpYear(), info.getFields().get(2));
            bindChipView(view.getCardholder(), info.getFields().get(3));

            // TODO(crbug.com/969708): Replace placeholder icon with data-specific icon.
            view.setIcon(AppCompatResources.getDrawable(
                    view.getContext(), R.drawable.infobar_autofill_cc));
        }

        void bindChipView(ChipView chip, UserInfoField field) {
            chip.getPrimaryTextView().setText(field.getDisplayText());
            chip.getPrimaryTextView().setContentDescription(field.getA11yDescription());
            if (!field.isSelectable() || field.getDisplayText().isEmpty()) {
                chip.setVisibility(View.GONE);
                return;
            }
            chip.setVisibility(View.VISIBLE);
            chip.setOnClickListener(src -> field.triggerSelection());
            chip.setClickable(true);
            chip.setEnabled(true);
        }
    }

    static void initializeView(RecyclerView view, AccessorySheetTabModel model) {
        view.setAdapter(new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model, AccessorySheetDataPiece::getType,
                        AccessorySheetTabViewBinder.ElementViewHolder::bind),
                CreditCardAccessorySheetViewBinder::create));
    }
}
