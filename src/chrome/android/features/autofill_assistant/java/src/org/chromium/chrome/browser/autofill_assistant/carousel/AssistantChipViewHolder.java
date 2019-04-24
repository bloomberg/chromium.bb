// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.autofill_assistant.R;

/**
 * The {@link ViewHolder} responsible for reflecting an {@link AssistantChip} to a {@link
 * TextView}.
 */
class AssistantChipViewHolder extends ViewHolder {
    private final TextView mText;

    private AssistantChipViewHolder(TextView itemView) {
        super(itemView);
        mText = itemView;
    }

    static AssistantChipViewHolder create(ViewGroup parent, int viewType) {
        LayoutInflater layoutInflater = LayoutInflater.from(parent.getContext());
        int resId = -1;
        switch (viewType) {
            // TODO: inflate normal chrome buttons instead.
            case AssistantChip.Type.CHIP_ASSISTIVE:
                resId = R.layout.autofill_assistant_chip_assistive;
                break;
            case AssistantChip.Type.BUTTON_FILLED_DISABLED:
            case AssistantChip.Type.BUTTON_FILLED_BLUE:
                resId = R.layout.autofill_assistant_button_filled;
                break;
            case AssistantChip.Type.BUTTON_HAIRLINE:
                resId = R.layout.autofill_assistant_button_hairline;
                break;
            default:
                assert false : "Unsupported view type " + viewType;
        }

        TextView view = (TextView) layoutInflater.inflate(resId, /* root= */ null);
        if (viewType == AssistantChip.Type.BUTTON_FILLED_DISABLED) {
            view.setEnabled(false);
        }

        return new AssistantChipViewHolder(view);
    }

    public void bind(AssistantChip chip) {
        mText.setText(chip.getText());
        mText.setOnClickListener(ignoredView -> chip.getSelectedListener().run());
    }
}
