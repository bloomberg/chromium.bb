// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.autofill_assistant.R;

/**
 * The {@link ViewHolder} responsible for reflecting an {@link AssistantChip} to a {@link
 * ButtonView}.
 */
class AssistantChipViewHolder extends ViewHolder {
    final ButtonView mView;

    private AssistantChipViewHolder(ButtonView view) {
        super(view);
        mView = view;
    }

    static AssistantChipViewHolder create(ViewGroup parent, int viewType) {
        LayoutInflater layoutInflater = LayoutInflater.from(parent.getContext());
        ButtonView view = null;
        switch (viewType % AssistantChip.Type.NUM_ENTRIES) {
            case AssistantChip.Type.CHIP_ASSISTIVE:
                view = (ButtonView) layoutInflater.inflate(
                        R.layout.autofill_assistant_button_assistive, /* root= */ null);
                break;
            case AssistantChip.Type.BUTTON_FILLED_BLUE:
                view = (ButtonView) layoutInflater.inflate(
                        R.layout.autofill_assistant_button_filled, /* root= */ null);
                break;
            case AssistantChip.Type.BUTTON_HAIRLINE:
                view = (ButtonView) layoutInflater.inflate(
                        R.layout.autofill_assistant_button_hairline, /* root= */ null);
                break;
            default:
                assert false : "Unsupported view type " + viewType;
        }

        view.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        if (viewType >= AssistantChip.Type.NUM_ENTRIES) {
            view.setEnabled(false);
        }

        return new AssistantChipViewHolder(view);
    }

    static int getViewType(AssistantChip chip) {
        // We add AssistantChip.Type.CHIP_TYPE_NUMBER to differentiate between enabled and disabled
        // chips of the same type. Ideally, we should return a (type, disabled) tuple but
        // RecyclerView does not allow that.
        if (chip.isDisabled()) {
            return chip.getType() + AssistantChip.Type.NUM_ENTRIES;
        }

        return chip.getType();
    }

    public void bind(AssistantChip chip) {
        String text = chip.getText();
        if (text.isEmpty()) {
            mView.getPrimaryTextView().setVisibility(View.GONE);
        } else {
            mView.getPrimaryTextView().setText(text);
            mView.getPrimaryTextView().setVisibility(View.VISIBLE);
        }

        mView.setOnClickListener(ignoredView -> chip.getSelectedListener().run());
        mView.setIcon(getIconResource(chip.getIcon()), /* tintWithTextColor= */ true);
    }

    private int getIconResource(@AssistantChip.Icon int icon) {
        switch (icon) {
            case AssistantChip.Icon.CLEAR:
                return R.drawable.ic_clear_black_24dp;
            case AssistantChip.Icon.DONE:
                return R.drawable.ic_done_black_24dp;
            case AssistantChip.Icon.REFRESH:
                return R.drawable.ic_refresh_black_24dp;
            default:
                return ButtonView.INVALID_ICON_ID;
        }
    }
}
